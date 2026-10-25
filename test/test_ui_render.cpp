// Runs the production UI and LVGL renderer; only clock and LCD transport are emulated.
#if defined(UI_RENDER_HOST)
#include "ui_main.h"
#include "ui_clock.h"
#include "ui_navigation.h"
#include "ui_ns_view.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

namespace {
uint32_t ticks;
alignas(8) unsigned char extra_pool[LV_MEM_POOL_EXPAND_SIZE];
uint16_t draw_buffers[2][320 * 16];
uint16_t framebuffer[320 * 170];
unsigned flushes;

void flush(lv_display_t* display, const lv_area_t* area, uint8_t* pixels) {
    const int width = area->x2 - area->x1 + 1;
    for (int y = area->y1; y <= area->y2; ++y) {
        std::memcpy(&framebuffer[y * 320 + area->x1], pixels, width * sizeof(uint16_t));
        pixels += width * sizeof(uint16_t);
    }
    ++flushes;
    lv_display_flush_ready(display);
}

void advance(uint32_t duration) {
    while (duration) {
        const uint32_t step = duration < 10 ? duration : 10;
        ticks += step;
        duration -= step;
        lv_timer_handler();
    }
}

void screenshot(const char* path) {
    FILE* out = std::fopen(path, "wb");
    if (!out) std::abort();
    std::fprintf(out, "P6\n320 170\n255\n");
    for (uint16_t pixel : framebuffer) {
        unsigned char rgb[] = {
            static_cast<unsigned char>(((pixel >> 11) & 31) * 255 / 31),
            static_cast<unsigned char>(((pixel >> 5) & 63) * 255 / 63),
            static_cast<unsigned char>((pixel & 31) * 255 / 31)
        };
        std::fwrite(rgb, 1, sizeof(rgb), out);
    }
    std::fclose(out);
}
}

bool getLocalTime(tm* info, uint32_t) {
    const time_t epoch = std::time(nullptr);
    *info = *std::localtime(&epoch);
    return true;
}

int main(int argc, char** argv) {
    std::setbuf(stdout, nullptr);
    lv_init();
    if (!lv_mem_add_pool(extra_pool, sizeof(extra_pool))) return 2;
    // A wider glucose label needs a contiguous ARGB buffer larger than 64 KiB.
    void* large_layer = lv_malloc(128 * 1024);
    if (!large_layer) return 4;
    lv_free(large_layer);
    // Emulate memory retained by inactive screens and exercise fragmentation pressure.
    if (argc > 2 && !lv_malloc(std::strtoul(argv[2], nullptr, 10))) return 6;
    lv_tick_set_cb([]() -> uint32_t { return ticks; });
    auto* display = lv_display_create(320, 170);
    lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(display, draw_buffers[0], draw_buffers[1], sizeof(draw_buffers[0]), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(display, flush);
    auto* main_screen = ui_main_create();
    auto* clock_screen = ui_clock_create();
    ui_navigation::init(nullptr);
    ui_navigation::update(false, false);
    NightscoutData data;
    data.ok = true;
    data.iob = 1.25f;
    data.cob = 20;
    data.update_time_ms = uint64_t(std::time(nullptr)) * 1000;
    const float bounds[] = {3.5f, 4.5f, 7.5f, 9.5f};
    std::memcpy(data.sgvBounds, bounds, sizeof(bounds));
    const char* trends[] = {"Flat", "FortyFiveUp", "SingleUp", "DoubleUp", "TripleUp",
                            "FortyFiveDown", "SingleDown", "DoubleDown", "TripleDown", "None"};
    constexpr uint32_t frame_ms = 250;
    // Four full glucose sweeps per screen, including preemption of 300 ms animations.
    for (int screen = 0; screen < 2; ++screen) {
        lv_screen_load(screen ? clock_screen : main_screen);
        size_t free_after_first_sweep = 0;
        for (unsigned index = 0; index <= 560; ++index) {
            const unsigned glucose_step = index % 140;
            data.sgv_mmol = float(30 + (glucose_step <= 70 ? glucose_step : 140 - glucose_step)) / 10;
            data.delta_mmol = glucose_step < 70 ? 0.1f : -0.1f;
            const unsigned battery_step = index % 40;
            data.batteryPct = 5 * int(battery_step <= 20 ? battery_step : 40 - battery_step);
            std::snprintf(data.trend, sizeof(data.trend), "%s", trends[index % 10]);
            if (index % 20 == 0) std::printf("screen=%d frame=%u glucose=%.1f trend=%s\n",
                screen, index, data.sgv_mmol, data.trend);
            if (screen) clock_update_full(data);
            else main_update_full(data);
            advance(frame_ms);
            if (lv_mem_test() != LV_RESULT_OK) return 3;
            if (index && index % 140 == 0) {
                lv_mem_monitor_t memory;
                lv_mem_monitor(&memory);
                std::printf("sweep=%u free=%lu largest=%lu\n", index / 140,
                    (unsigned long)memory.free_size, (unsigned long)memory.free_biggest_size);
                if (!free_after_first_sweep) free_after_first_sweep = memory.free_size;
                // Allow one animation node of timing variation, but no per-frame memory leak.
                else if (memory.free_size + 256 < free_after_first_sweep) return 5;
            }
            if (argc > 1 && (index == 0 || index == 2 || index == 5
                    || index == 28 || index == 70 || index == 140)) {
                char filename[256];
                std::snprintf(filename, sizeof(filename), "%s/screen-%d-frame-%u.ppm", argv[1], screen, index);
                screenshot(filename);
            }
        }
        lv_mem_monitor_t memory;
        lv_mem_monitor(&memory);
        std::printf("screen=%d free=%lu largest=%lu flushes=%u\n", screen,
            (unsigned long)memory.free_size, (unsigned long)memory.free_biggest_size, flushes);
    }
    std::puts("PASS: production UI rendered all glucose values and Nightscout trends");
}
#endif
