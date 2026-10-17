#pragma once

#include "trend_arrow_geometry.h"
#include <lvgl.h>
#include <cmath>

// Одна анимация управляет всей группой, сохраняя расстояние между стрелками.
class TrendArrows {
public:
    void create(lv_obj_t* parent, const trend_arrow::Geometry& geometry,
                int center_x, int center_y, lv_color_t color) {
        geometry_ = geometry;
        center_x_ = center_x;
        center_y_ = center_y;
        const int tip = 2 * geometry.half_length;
        const int pivot_y = geometry.half_head;
        points_[0] = {0, pivot_y};
        points_[1] = {tip, pivot_y};
        points_[2] = {tip - geometry.head_length, 0};
        points_[3] = {tip, pivot_y};
        points_[4] = {tip - geometry.head_length, 2 * pivot_y};
        for (auto& arrow : arrows_) {
            arrow = lv_line_create(parent);
            lv_line_set_points(arrow, points_, 5);
            lv_obj_set_style_line_width(arrow, geometry.line_width, LV_PART_MAIN);
            lv_obj_set_style_line_rounded(arrow, true, LV_PART_MAIN);
            lv_obj_set_style_line_color(arrow, color, LV_PART_MAIN);
            lv_obj_set_style_transform_pivot_x(arrow, geometry.half_length, LV_PART_MAIN);
            lv_obj_set_style_transform_pivot_y(arrow, pivot_y, LV_PART_MAIN);
            lv_obj_add_flag(arrow, LV_OBJ_FLAG_HIDDEN);
        }
    }

    void set_color(lv_color_t color) {
        for (auto* arrow : arrows_)
            if (arrow) lv_obj_set_style_line_color(arrow, color, LV_PART_MAIN);
    }

    void set(int angle_deg, int count, uint32_t duration_ms) {
        const int target = trend_arrow::normalize(angle_deg * 10);
        count = trend_arrow::limit_count(count);
        if (target == target10_ && count == count_) return;
        lv_anim_del(this, animate);
        target10_ = target;
        count_ = count;
        const int delta = trend_arrow::rotation_delta(angle10_, target);
        if (!count || !delta || !duration_ms) {
            render(count ? target : angle10_);
            return;
        }
        lv_anim_t animation;
        lv_anim_init(&animation);
        lv_anim_set_var(&animation, this);
        lv_anim_set_exec_cb(&animation, animate);
        lv_anim_set_time(&animation, duration_ms);
        lv_anim_set_values(&animation, angle10_, angle10_ + delta);
        lv_anim_set_path_cb(&animation, lv_anim_path_ease_in_out);
        lv_anim_start(&animation);
    }

private:
    trend_arrow::Geometry geometry_{};
    lv_point_precise_t points_[5]{};
    lv_obj_t* arrows_[3]{};
    int center_x_ = 0, center_y_ = 0;
    int angle10_ = 0, target10_ = 0, count_ = 0;

    static void animate(void* group, int32_t angle10) {
        static_cast<TrendArrows*>(group)->render(angle10);
    }

    void render(int angle10) {
        angle10_ = trend_arrow::normalize(angle10);
        const float radians = float(angle10_) * 3.14159265358979323846f / 1800;
        const auto frame = trend_arrow::frame(geometry_, count_, std::cos(radians), std::sin(radians));
        for (int i = 0; i < 3; ++i) {
            if (!arrows_[i]) continue;
            if (i >= count_) { lv_obj_add_flag(arrows_[i], LV_OBJ_FLAG_HIDDEN); continue; }
            lv_obj_set_pos(arrows_[i], center_x_ + frame.x[i] - geometry_.half_length,
                           center_y_ + frame.y[i] - geometry_.half_head);
            lv_obj_set_style_transform_angle(arrows_[i], angle10_, LV_PART_MAIN);
            lv_obj_set_style_transform_scale_x(arrows_[i], frame.scale, LV_PART_MAIN);
            lv_obj_set_style_transform_scale_y(arrows_[i], frame.scale, LV_PART_MAIN);
            lv_obj_clear_flag(arrows_[i], LV_OBJ_FLAG_HIDDEN);
        }
    }
};
