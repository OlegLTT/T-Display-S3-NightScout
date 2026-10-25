#include "trend_arrow_geometry.h"

using namespace trend_arrow;

static_assert(rotation_delta(-900, -900) == 0, "SingleUp to DoubleUp must not spin");
static_assert(rotation_delta(900, 900) == 0, "SingleDown to DoubleDown must not spin");
static_assert(rotation_delta(-900, 900) == 1800, "Up to down rotates through right");
static_assert(rotation_delta(900, -900) == -1800, "Down to up rotates through right");
static_assert(rotation_delta(-900, -450) == 450, "Adjacent trends use a short turn");
static_assert(rotation_delta(3500, 100) == 200, "Animation crosses angle wrap smoothly");

constexpr auto main_up = frame(main_geometry, 2, 0, -1);
constexpr auto main_down = frame(main_geometry, 2, 0, 1);
constexpr auto clock_up = frame(clock_geometry, 2, 0, -1);
static_assert(main_up.scale == 256 && main_up.x[0] == -17 && main_up.x[1] == 17,
    "Large double arrows keep full size and have separate heads");
static_assert(main_down.x[0] == 17 && main_down.x[1] == -17,
    "Down arrows stay symmetrically spaced");
static_assert(clock_up.scale == 256 && clock_up.x[0] == -10 && clock_up.x[1] == 10,
    "Clock double arrows fit at full size");
static_assert(main_geometry.spacing > 2 * main_geometry.half_head + main_geometry.line_width,
    "Large arrowheads must not touch");
static_assert(clock_geometry.spacing > 2 * clock_geometry.half_head + clock_geometry.line_width,
    "Clock arrowheads must not touch");

constexpr bool fits(const Geometry& geometry, int count, float cosine, float sine) {
    const auto placement = frame(geometry, count, cosine, sine);
    const float scale = float(placement.scale) / 256;
    const float tip = float(geometry.half_length);
    const float head = tip - geometry.head_length;
    const float x[] = {-tip, tip, head, tip, head};
    const float y[] = {0, 0, -float(geometry.half_head), 0, float(geometry.half_head)};
    const float radius = float(geometry.line_width) * scale / 2;
    for (int arrow = 0; arrow < count; ++arrow) {
        if (arrow) {
            const float separation = -sine * (placement.x[arrow] - placement.x[arrow - 1])
                + cosine * (placement.y[arrow] - placement.y[arrow - 1]);
            if (separation <= (2 * geometry.half_head + geometry.line_width) * scale) return false;
        }
        for (int point = 0; point < 5; ++point) {
            if (absolute((x[point] * cosine - y[point] * sine) * scale + placement.x[arrow])
                    + radius > float(geometry.width) / 2) return false;
            if (absolute((x[point] * sine + y[point] * cosine) * scale + placement.y[arrow])
                    + radius > float(geometry.height) / 2) return false;
        }
    }
    return true;
}

constexpr bool all_frames_fit(const Geometry& geometry) {
    // Cardinal directions, diagonals and intermediate animation frames.
    const float cosines[] = {1, 0.9659258f, 0.8660254f, 0.7071068f, 0.5f, 0.2588190f, 0};
    const float sines[] = {0, 0.2588190f, 0.5f, 0.7071068f, 0.8660254f, 0.9659258f, 1};
    for (int count = 1; count <= 3; ++count)
        for (unsigned angle = 0; angle < sizeof(cosines) / sizeof(cosines[0]); ++angle)
            for (int x_sign = -1; x_sign <= 1; x_sign += 2)
                for (int y_sign = -1; y_sign <= 1; y_sign += 2)
                    if (!fits(geometry, count, x_sign * cosines[angle], y_sign * sines[angle])) return false;
    return true;
}
static_assert(all_frames_fit(main_geometry), "Large arrows must fit and stay separate throughout rotation");
static_assert(all_frames_fit(clock_geometry), "Clock arrows must fit and stay separate throughout rotation");
static_assert(frame(clock_geometry, 2, 1, 0).scale < 256,
    "Two arrows shrink during rotation instead of clipping the 26-pixel clock row");
static_assert(frame(main_geometry, 3, 0, 1).scale < 256,
    "Triple arrows must fit in the same area");
