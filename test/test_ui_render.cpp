// Runs the production UI and LVGL renderer; only clock and LCD transport are emulated.
#if defined(UI_RENDER_HOST)
#include "ui_main.h"
#include "ui_clock.h"
#include "ui_navigation.h"
#include "ui_layout.h"
#include "ui_ns_view.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <limits>

namespace {
uint32_t ticks;
#if LV_MEM_POOL_EXPAND_SIZE > 0
alignas(8) unsigned char extra_pool[LV_MEM_POOL_EXPAND_SIZE];
#endif
constexpr int screen_height =
#if defined(BOARD_USES_M5UNIFIED)
    240;
#else
    170;
#endif
uint16_t draw_buffers[2][320 * 16];
uint16_t framebuffer[320 * screen_height];
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
    std::fprintf(out, "P6\n320 %d\n255\n", screen_height);
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

lv_obj_t* find_label(lv_obj_t* root, const char* text) {
    if (lv_obj_check_type(root, &lv_label_class) && std::strcmp(lv_label_get_text(root), text) == 0)
        return root;
    for (uint32_t i = 0; i < lv_obj_get_child_count(root); ++i)
        if (auto* result = find_label(lv_obj_get_child(root, i), text)) return result;
    return nullptr;
}

uint32_t rgb(lv_color_t color) { return lv_color_to_u32(color) & 0xFFFFFF; }

bool check_delta_colors(lv_obj_t* screens[2], const char* output) {
    struct Example { float value; const char* text; uint32_t color; };
    const Example examples[] = {
        {0.0f, "+0.0", 0x00FF00}, {-0.01f, "+0.0", 0x00FF00},
        {0.2f, "+0.2", 0x00FF00}, {-0.2f, "-0.2", 0x00FF00},
        {0.3f, "+0.3", 0xFFFF00}, {-0.3f, "-0.3", 0xFFFF00},
        {0.5f, "+0.5", 0xFFFF00}, {-0.5f, "-0.5", 0xFFFF00},
        {0.6f, "+0.6", 0xFF0000}, {-0.6f, "-0.6", 0xFF0000},
        {0.7f, "+0.7", 0xFF0000}, {-0.7f, "-0.7", 0xFF0000},
        {0.28f, "+0.3", 0xFFFF00}, {-0.28f, "-0.3", 0xFFFF00},
        {0.58f, "+0.6", 0xFF0000}, {-0.58f, "-0.6", 0xFF0000},
        {std::numeric_limits<float>::quiet_NaN(), "--", 0xC8C8C8},
        {std::numeric_limits<float>::infinity(), "--", 0xC8C8C8}
    };
    NightscoutData data;
    data.ok = true;
    data.sgv_mmol = 3.0f; // A red glucose zone must not force the delta to red.
    data.iob = 1.0f;
    data.cob = 20.0f;
    data.batteryPct = 80;
    data.update_time_ms = uint64_t(std::time(nullptr)) * 1000;
    const float bounds[] = {3.5f, 4.5f, 7.5f, 9.5f};
    std::memcpy(data.sgvBounds, bounds, sizeof(bounds));
    for (int screen = 0; screen < 2; ++screen) {
        lv_screen_load(screens[screen]);
        for (unsigned index = 0; index < sizeof(examples) / sizeof(examples[0]); ++index) {
            const auto& example = examples[index];
            data.delta_mmol = example.value;
            const auto view = ns_make_view(data);
            if (std::strcmp(view.delta, example.text) || rgb(view.deltaCol) != example.color) {
                std::printf("Delta view mismatch at case %u: %s color=%06lX\n", index, view.delta,
                            (unsigned long)rgb(view.deltaCol));
                return false;
            }
            if (screen) clock_update_full(data);
            else main_update_full(data);
            auto* label = find_label(screens[screen], example.text);
            if (!label || rgb(lv_obj_get_style_text_color(label, LV_PART_MAIN)) != example.color) {
                std::printf("Delta label mismatch on screen %d at case %u\n", screen, index);
                return false;
            }
            advance(250);
            if (output && (index == 2 || index == 4 || index == 8)) {
                char path[256];
                std::snprintf(path, sizeof(path), "%s/delta-screen-%d-case-%u.ppm", output, screen, index);
                screenshot(path);
            }
        }
    }
    // At exactly 15 minutes and without a usable measurement time, the delta is grey.
    data.delta_mmol = 0.7f;
    data.update_time_ms = uint64_t(std::time(nullptr) - 900) * 1000;
    auto view = ns_make_view(data);
    if (std::strcmp(view.delta, "--") || rgb(view.deltaCol) != 0x787878) return false;
    data.update_time_ms = 0;
    view = ns_make_view(data);
    if (std::strcmp(view.delta, "--") || rgb(view.deltaCol) != 0x787878) return false;
    data.update_time_ms = uint64_t(std::time(nullptr)) * 1000;
    data.stale15 = true;
    view = ns_make_view(data);
    if (std::strcmp(view.delta, "--") || rgb(view.deltaCol) != 0x787878) return false;
    std::puts("PASS: delta colors, sign, rounding, missing/stale data on main and clock screens");
    return true;
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
#if LV_MEM_POOL_EXPAND_SIZE > 0
    if (!lv_mem_add_pool(extra_pool, sizeof(extra_pool))) return 2;
    // A wider glucose label needs a contiguous ARGB buffer larger than 64 KiB.
    void* large_layer = lv_malloc(128 * 1024);
    if (!large_layer) return 4;
    lv_free(large_layer);
#endif
    // Emulate memory retained by inactive screens and exercise fragmentation pressure.
    if (argc > 2 && !lv_malloc(std::strtoul(argv[2], nullptr, 10))) return 6;
    lv_tick_set_cb([]() -> uint32_t { return ticks; });
    auto* display = lv_display_create(320, screen_height);
    lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(display, draw_buffers[0], draw_buffers[1], sizeof(draw_buffers[0]), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(display, flush);
    auto* main_screen = ui_main_create();
    auto* clock_screen = ui_clock_create();
    ui_navigation::init(nullptr);
    ui_navigation::update(false, false);
#if defined(BOARD_USES_M5UNIFIED)
    if (ui_layout::content_height() != 196
        || lv_obj_get_style_pad_bottom(main_screen, LV_PART_MAIN) != 44
        || lv_obj_get_style_pad_bottom(clock_screen, LV_PART_MAIN) != 44) return 8;
    ui_navigation::set_alarm_status(true, true, false, true, 0, 0);
    if (!find_label(lv_layer_top(), "Звук включён")) return 9;
    ui_navigation::set_alarm_status(true, true, false, true, 300, 5);
    if (!find_label(lv_layer_top(), "Отсрочка 5 мин | 5:00")) return 10;
    ui_navigation::set_alarm_status(true, true, false, true, 0, 0);
#endif
    lv_obj_t* screens[] = {main_screen, clock_screen};
    if (!check_delta_colors(screens, argc > 1 ? argv[1] : nullptr)) return 7;
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
