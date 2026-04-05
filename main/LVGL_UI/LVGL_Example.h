#pragma once

#include "esp_err.h"
#include "lvgl.h"

esp_err_t lvgl_oled_init(void);
lv_disp_t *lvgl_oled_get_display(void);
void lvgl_oled_lock(void);
void lvgl_oled_unlock(void);

void lvgl_update_text(const char *text);
void lvgl_caption_set_committed(const char *text);
void lvgl_caption_set_live(const char *text);
void lvgl_caption_clear_live(void);
void lvgl_caption_show_error(const char *text);

