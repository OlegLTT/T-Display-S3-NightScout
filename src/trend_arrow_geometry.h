#pragma once

#include <cstdint>

namespace trend_arrow {
struct Geometry {
    int half_length, head_length, half_head, line_width;
    int spacing, width, height;
};

// Расстояние между осями больше ширины наконечника вместе с толщиной линии.
constexpr Geometry main_geometry = {30, 12, 12, 8, 34, 80, 80};
constexpr Geometry clock_geometry = {9, 9, 8, 3, 20, 56, 26};

constexpr int normalize(int angle10) { return (angle10 % 3600 + 3600) % 3600; }
constexpr int limit_count(int count) { return count < 0 ? 0 : count > 3 ? 3 : count; }
constexpr float absolute(float value) { return value < 0 ? -value : value; }
constexpr int round_pixel(float value) { return int(value < 0 ? value - 0.5f : value + 0.5f); }
constexpr int shortest_turn(int delta) {
    return delta > 1800 ? delta - 3600 : delta < -1800 ? delta + 3600 : delta;
}
constexpr int rotation_delta(int start10, int target10) {
    return absolute(float(shortest_turn(normalize(target10) - normalize(start10)))) == 1800
        ? (normalize(start10) < 1800 ? -1800 : 1800)
        : shortest_turn(normalize(target10) - normalize(start10));
}

struct Frame {
    int scale = 256;
    int x[3] = {};
    int y[3] = {};
};

#if __cplusplus >= 201402L
#define TREND_CONSTEXPR14 constexpr
#else
#define TREND_CONSTEXPR14 inline
#endif

// Поворот и разнос выполняются вместе. Масштаб ограничивает весь рисунок,
// включая закруглённые концы линий и запас на округление координат.
TREND_CONSTEXPR14 Frame frame(const Geometry& geometry, int count, float cosine, float sine) {
    Frame result;
    count = limit_count(count);
    if (!count) return result;
    const float tip = float(geometry.half_length);
    const float head = tip - geometry.head_length;
    const float px[5] = {-tip, tip, head, tip, head};
    const float py[5] = {0, 0, -float(geometry.half_head), 0, float(geometry.half_head)};
    float radius_x = 0, radius_y = 0;
    for (int i = 0; i < count; ++i) {
        const float offset = (float(i) - float(count - 1) / 2) * geometry.spacing;
        for (int point = 0; point < 5; ++point) {
            const float x = absolute(px[point] * cosine - py[point] * sine - sine * offset);
            const float y = absolute(px[point] * sine + py[point] * cosine + cosine * offset);
            if (x > radius_x) radius_x = x;
            if (y > radius_y) radius_y = y;
        }
    }
    const float stroke_margin = float(geometry.line_width) / 2;
    radius_x += stroke_margin;
    radius_y += stroke_margin;
    float scale = 1;
    const float available_x = float(geometry.width) / 2 - 1;
    const float available_y = float(geometry.height) / 2 - 1;
    if (radius_x > available_x) scale = available_x / radius_x;
    if (radius_y * scale > available_y) scale = available_y / radius_y;
    result.scale = int(scale * 256);
    if (result.scale < 1) result.scale = 1;
    scale = float(result.scale) / 256;
    for (int i = 0; i < count; ++i) {
        const float offset = (float(i) - float(count - 1) / 2) * geometry.spacing * scale;
        result.x[i] = round_pixel(-sine * offset);
        result.y[i] = round_pixel(cosine * offset);
    }
    return result;
}

#undef TREND_CONSTEXPR14
} // namespace trend_arrow
