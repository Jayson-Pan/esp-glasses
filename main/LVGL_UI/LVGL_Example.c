#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "esp_log.h"
#include "lvgl.h"
#include "LVGL_Driver.h"
#include "ST7789.h"
#include "LVGL_Example.h"
#include "fonts/font_20.h"
#include "fonts/nav_icon.h"
#include "fonts/vehicle.h"

#define LVGL_TASK_DELAY_MS   10
#define LVGL_TASK_STACK_SIZE 4096
#define LVGL_TASK_PRIORITY   4

static const char *TAG = "LVGL_UI";

static SemaphoreHandle_t s_lvgl_mutex = NULL;
static TaskHandle_t s_lvgl_task_handle = NULL;
static lv_disp_t *s_disp = NULL;
static bool s_initialized = false;

void lvgl_oled_lock(void);
void lvgl_oled_unlock(void);

static lv_obj_t *s_left_container = NULL;
static lv_obj_t *s_right_container = NULL;
static lv_obj_t *s_current_road_label = NULL;
static lv_obj_t *s_nav_icon_label = NULL;
static lv_obj_t *s_next_road_label = NULL;
static lv_obj_t *s_remaining_dist_label = NULL;
static lv_obj_t *s_next_dist_label = NULL;
static lv_obj_t *s_vehicle_icon_label = NULL;

static const char *DEFAULT_CURRENT_ROAD = "当前道路";
static const char *DEFAULT_NEXT_ROAD = "等待数据";
static const char *DEFAULT_NEXT_DISTANCE = "---米";
static const char *DEFAULT_REMAINING_DISTANCE = "---km";

static void lvgl_task(void *param);
static void create_navigation_ui(void);
static void update_label_locked(lv_obj_t *label, const char *text, const char *fallback);
static const char *get_nav_icon_symbol(int icon_type);
static void format_distance(char *buf, size_t buf_len, int distance_meters);
static bool ensure_mutex(void);

static bool ensure_mutex(void)
{
    if (s_lvgl_mutex == NULL) {
        s_lvgl_mutex = xSemaphoreCreateRecursiveMutex();
        if (s_lvgl_mutex == NULL) {
            ESP_LOGE(TAG, "创建 LVGL 互斥锁失败");
            return false;
        }
    }
    return true;
}

esp_err_t lvgl_oled_init(void)
{
    if (s_initialized) {
        return ESP_OK;
    }

    if (!ensure_mutex()) {
        return ESP_ERR_NO_MEM;
    }

    LCD_Init();
    LVGL_Init();

    s_disp = lv_disp_get_default();
    if (s_disp == NULL) {
        ESP_LOGE(TAG, "获取默认 LVGL 显示对象失败");
        return ESP_FAIL;
    }

    lvgl_oled_lock();
    lv_obj_t *scr = lv_disp_get_scr_act(s_disp);
    lv_obj_clean(scr);
    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    create_navigation_ui();
    /* 触发一次立即刷新，确保首帧覆盖整个屏幕，避免残留旧画面 */
    lv_timer_handler();
    lvgl_oled_unlock();

    if (s_lvgl_task_handle == NULL) {
        BaseType_t ret = xTaskCreate(lvgl_task, "lvgl_task", LVGL_TASK_STACK_SIZE, NULL, LVGL_TASK_PRIORITY, &s_lvgl_task_handle);
        if (ret != pdPASS) {
            ESP_LOGE(TAG, "创建 LVGL 任务失败");
            vSemaphoreDelete(s_lvgl_mutex);
            s_lvgl_mutex = NULL;
            return ESP_FAIL;
        }
    }

    s_initialized = true;
    ESP_LOGI(TAG, "LVGL 导航界面初始化完成");
    return ESP_OK;
}

lv_disp_t *lvgl_oled_get_display(void)
{
    return s_disp;
}

void lvgl_oled_lock(void)
{
    if (s_lvgl_mutex) {
        xSemaphoreTakeRecursive(s_lvgl_mutex, portMAX_DELAY);
    }
}

void lvgl_oled_unlock(void)
{
    if (s_lvgl_mutex) {
        xSemaphoreGiveRecursive(s_lvgl_mutex);
    }
}

void lvgl_update_current_road(const char *road_name)
{
    if (!s_initialized) {
        return;
    }
    lvgl_oled_lock();
    update_label_locked(s_current_road_label, road_name, DEFAULT_CURRENT_ROAD);
    lvgl_oled_unlock();
}

void lvgl_update_next_road(const char *road_name)
{
    if (!s_initialized) {
        return;
    }
    lvgl_oled_lock();
    update_label_locked(s_next_road_label, road_name, DEFAULT_NEXT_ROAD);
    lvgl_oled_unlock();
}

void lvgl_update_nav_icon(int icon_type)
{
    if (!s_initialized || s_nav_icon_label == NULL) {
        return;
    }
    const char *symbol = get_nav_icon_symbol(icon_type);
    lvgl_oled_lock();
    lv_label_set_text(s_nav_icon_label, symbol);
    lvgl_oled_unlock();
}

void lvgl_update_next_distance(int distance_meters)
{
    if (!s_initialized || s_next_dist_label == NULL) {
        return;
    }
    char buf[16];
    format_distance(buf, sizeof(buf), distance_meters);
    lvgl_oled_lock();
    lv_label_set_text(s_next_dist_label, buf);
    lvgl_oled_unlock();
}

void lvgl_update_remaining_distance(int distance_meters)
{
    if (!s_initialized || s_remaining_dist_label == NULL) {
        return;
    }
    char buf[16];
    format_distance(buf, sizeof(buf), distance_meters);
    lvgl_oled_lock();
    lv_label_set_text(s_remaining_dist_label, buf);
    lvgl_oled_unlock();
}

void lvgl_update_text(const char *text)
{
    if (!s_initialized) {
        return;
    }
    lvgl_oled_lock();
    update_label_locked(s_current_road_label, text, DEFAULT_CURRENT_ROAD);
    lvgl_oled_unlock();
}

static void lvgl_task(void *param)
{
    const TickType_t delay = pdMS_TO_TICKS(LVGL_TASK_DELAY_MS);
    while (true) {
        lvgl_oled_lock();
        lv_timer_handler();
        lvgl_oled_unlock();
        vTaskDelay(delay);
    }
}

static void create_navigation_ui(void)
{
    lv_coord_t width = lv_disp_get_hor_res(s_disp);
    lv_coord_t height = lv_disp_get_ver_res(s_disp);

    lv_coord_t left_width = width / 2 - 1;
    if (left_width < 0) {
        left_width = width;
    }
    lv_coord_t right_width = width - left_width - 2;
    if (right_width < 0) {
        right_width = width / 2;
    }

    s_left_container = lv_obj_create(lv_disp_get_scr_act(s_disp));
    lv_obj_remove_style_all(s_left_container);
    lv_obj_set_size(s_left_container, left_width, height);
    lv_obj_set_pos(s_left_container, 0, 0);
    lv_obj_set_style_bg_opa(s_left_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_left_container, 1, 0);
    lv_obj_set_style_border_color(s_left_container, lv_color_white(), 0);
    lv_obj_set_style_pad_all(s_left_container, 6, 0);
    lv_obj_set_style_pad_gap(s_left_container, 14, 0);
    lv_obj_set_style_pad_top(s_left_container, 16, 0);
    lv_obj_set_style_pad_bottom(s_left_container, 16, 0);
    lv_obj_set_style_text_color(s_left_container, lv_color_white(), 0);
    lv_obj_set_flex_flow(s_left_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_left_container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    s_next_road_label = lv_label_create(s_left_container);
    lv_label_set_text(s_next_road_label, DEFAULT_NEXT_ROAD);
    lv_obj_set_width(s_next_road_label, LV_PCT(100));
    lv_label_set_long_mode(s_next_road_label, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_style_text_font(s_next_road_label, &font_20, 0);
    lv_obj_set_style_text_align(s_next_road_label, LV_TEXT_ALIGN_CENTER, 0);

    s_nav_icon_label = lv_label_create(s_left_container);
    lv_label_set_text(s_nav_icon_label, ICON_FORWARD);
    lv_obj_set_width(s_nav_icon_label, LV_PCT(100));
    lv_obj_set_style_text_font(s_nav_icon_label, &nav_icon, 0);
    lv_obj_set_style_text_align(s_nav_icon_label, LV_TEXT_ALIGN_CENTER, 0);

    s_current_road_label = lv_label_create(s_left_container);
    lv_label_set_text(s_current_road_label, DEFAULT_CURRENT_ROAD);
    lv_obj_set_width(s_current_road_label, LV_PCT(100));
    lv_label_set_long_mode(s_current_road_label, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_style_text_font(s_current_road_label, &font_20, 0);
    lv_obj_set_style_text_align(s_current_road_label, LV_TEXT_ALIGN_CENTER, 0);

    s_right_container = lv_obj_create(lv_disp_get_scr_act(s_disp));
    lv_obj_remove_style_all(s_right_container);
    lv_obj_set_size(s_right_container, right_width, height);
    lv_obj_set_pos(s_right_container, left_width + 2, 0);
    lv_obj_set_style_bg_opa(s_right_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_right_container, 1, 0);
    lv_obj_set_style_border_color(s_right_container, lv_color_white(), 0);
    lv_obj_set_style_pad_all(s_right_container, 6, 0);
    lv_obj_set_style_pad_gap(s_right_container, 16, 0);
    lv_obj_set_style_pad_top(s_right_container, 16, 0);
    lv_obj_set_style_pad_bottom(s_right_container, 16, 0);
    lv_obj_set_style_text_color(s_right_container, lv_color_white(), 0);
    lv_obj_set_flex_flow(s_right_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_right_container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    s_remaining_dist_label = lv_label_create(s_right_container);
    lv_label_set_text(s_remaining_dist_label, DEFAULT_REMAINING_DISTANCE);
    lv_obj_set_width(s_remaining_dist_label, LV_PCT(100));
    lv_obj_set_style_text_font(s_remaining_dist_label, &font_20, 0);
    lv_obj_set_style_text_align(s_remaining_dist_label, LV_TEXT_ALIGN_CENTER, 0);

    /* 车辆图标，初始隐藏（空文本） */
    s_vehicle_icon_label = lv_label_create(s_right_container);
    lv_obj_set_width(s_vehicle_icon_label, LV_PCT(100));
    lv_obj_set_style_text_font(s_vehicle_icon_label, &vehicle, 0);
    lv_obj_set_style_text_align(s_vehicle_icon_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(s_vehicle_icon_label, "");

    s_next_dist_label = lv_label_create(s_right_container);
    lv_label_set_text(s_next_dist_label, DEFAULT_NEXT_DISTANCE);
    lv_obj_set_width(s_next_dist_label, LV_PCT(100));
    lv_obj_set_style_text_font(s_next_dist_label, &font_20, 0);
    lv_obj_set_style_text_align(s_next_dist_label, LV_TEXT_ALIGN_CENTER, 0);
}

static void update_label_locked(lv_obj_t *label, const char *text, const char *fallback)
{
    if (label == NULL) {
        return;
    }
    if (text != NULL && text[0] != '\0') {
        lv_label_set_text(label, text);
    } else if (fallback != NULL) {
        lv_label_set_text(label, fallback);
    } else {
        lv_label_set_text(label, "");
    }
}

static void format_distance(char *buf, size_t buf_len, int distance_meters)
{
    if (distance_meters < 0) {
        distance_meters = 0;
    }

    if (distance_meters >= 1000) {
        float km = distance_meters / 1000.0f;
        snprintf(buf, buf_len, "%.1fkm", km);
    } else {
        snprintf(buf, buf_len, "%dm", distance_meters);
    }
}

static const char *get_nav_icon_symbol(int icon_type)
{
    switch (icon_type) {
        case 2:  return ICON_LEFT;
        case 3:  return ICON_RIGHT;
        case 4:  return ICON_FRONT_LEFT;
        case 5:  return ICON_FRONT_RIGHT;
        case 6:  return ICON_REAR_LEFT;
        case 7:  return ICON_REAR_RIGHT;
        case 9:  return ICON_FORWARD;
        case 15: return ICON_END;
        default: return ICON_FORWARD;
    }
}

/* UTF-8 of U+E606 (默认 CAR 图标) */
/* 默认车辆图标（CAR） */
#define VEHICLE_CAR_SYMBOL CAR

void lvgl_set_vehicle_detect(bool detected)
{
    if (!s_initialized || s_vehicle_icon_label == NULL) {
        return;
    }
    lvgl_oled_lock();
    if (detected) {
        lv_label_set_text(s_vehicle_icon_label, VEHICLE_CAR_SYMBOL);
    } else {
        lv_label_set_text(s_vehicle_icon_label, "");
    }
    lvgl_oled_unlock();
}

