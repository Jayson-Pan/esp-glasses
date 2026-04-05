#pragma once

#include "esp_err.h"
#include "lvgl.h"

esp_err_t lvgl_oled_init(void);
lv_disp_t *lvgl_oled_get_display(void);
void lvgl_oled_lock(void);
void lvgl_oled_unlock(void);

void lvgl_update_current_road(const char *road_name);
void lvgl_update_next_road(const char *road_name);
void lvgl_update_next_distance(int distance_meters);
void lvgl_update_remaining_distance(int distance_meters);
void lvgl_update_text(const char *text);

