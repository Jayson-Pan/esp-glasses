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
#include "fonts/font_18.h"

#define LVGL_TASK_DELAY_MS   10
#define LVGL_TASK_STACK_SIZE 4096
#define LVGL_TASK_PRIORITY   4

#define UI_BG_COLOR_HEX          0x1A1A1A
#define UI_DIVIDER_COLOR_HEX     0x4A4A4A
#define UI_TEXT_COMMITTED_HEX    0xB8D3FF
#define UI_TEXT_LIVE_HEX         0xFFFFFF
#define UI_TEXT_ERROR_HEX        0xFF8A80
#define UI_STATUS_BAR_WIDTH      30
#define UI_DIVIDER_WIDTH         1
#define UI_VISIBLE_CROP_TOP_DIV  4
#define UI_DIALOG_GAP            6
#define UI_TEXT_BUF_LEN          512

static const char *TAG = "LVGL_UI";

static SemaphoreHandle_t s_lvgl_mutex = NULL;
static TaskHandle_t s_lvgl_task_handle = NULL;
static lv_disp_t *s_disp = NULL;
static bool s_initialized = false;

void lvgl_oled_lock(void);
void lvgl_oled_unlock(void);

static lv_obj_t *s_status_sidebar = NULL;
static lv_obj_t *s_main_text_area = NULL;
static lv_obj_t *s_dialog_label_top = NULL;
static lv_obj_t *s_dialog_label_mid = NULL;
static void lvgl_task(void *param);
static void create_smart_glasses_demo_ui(void);
static lv_obj_t *create_dialog_label(lv_obj_t *parent, lv_coord_t width, const char *text);
static void normalize_punctuation_to_ascii(const char *src, char *dst, size_t dst_len);
static void clear_caption_line_locked(lv_obj_t *label);
static void sanitize_display_text(const char *src, char *dst, size_t dst_len);
static void set_caption_line_locked(
    lv_obj_t *label,
    const char *text,
    const char *placeholder);
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
    lv_obj_set_style_bg_color(scr, lv_color_hex(UI_BG_COLOR_HEX), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    /* 创建智能眼镜演示界面：左侧状态栏 + 右侧对话区 */
    create_smart_glasses_demo_ui();
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
    ESP_LOGI(TAG, "LVGL 智能眼镜演示界面初始化完成");
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

void lvgl_update_text(const char *text)
{
    lvgl_caption_set_live(text);
}

void lvgl_caption_set_committed(const char *text)
{
    if (!s_initialized || s_dialog_label_top == NULL) {
        ESP_LOGW(TAG, "忽略定稿字幕更新，界面尚未初始化");
        return;
    }
    ESP_LOGI(TAG, "更新定稿字幕: %s", text == NULL ? "<placeholder>" : text);
    lvgl_oled_lock();
    lv_obj_set_style_text_color(s_dialog_label_top, lv_color_hex(UI_TEXT_COMMITTED_HEX), 0);
    set_caption_line_locked(s_dialog_label_top, text, NULL);
    lvgl_oled_unlock();
}

void lvgl_caption_set_live(const char *text)
{
    if (!s_initialized || s_dialog_label_mid == NULL) {
        ESP_LOGW(TAG, "忽略实时字幕更新，界面尚未初始化");
        return;
    }
    ESP_LOGI(TAG, "更新实时字幕: %s", text == NULL ? "<placeholder>" : text);
    lvgl_oled_lock();
    lv_obj_set_style_text_color(s_dialog_label_mid, lv_color_hex(UI_TEXT_LIVE_HEX), 0);
    set_caption_line_locked(s_dialog_label_mid, text, NULL);
    lvgl_oled_unlock();
}

void lvgl_caption_clear_live(void)
{
    if (!s_initialized || s_dialog_label_mid == NULL) {
        ESP_LOGW(TAG, "忽略清空实时字幕，界面尚未初始化");
        return;
    }
    ESP_LOGI(TAG, "清空实时字幕显示");
    lvgl_oled_lock();
    lv_obj_set_style_text_color(s_dialog_label_mid, lv_color_hex(UI_TEXT_LIVE_HEX), 0);
    clear_caption_line_locked(s_dialog_label_mid);
    lvgl_oled_unlock();
}

void lvgl_caption_show_error(const char *text)
{
    if (!s_initialized || s_dialog_label_mid == NULL) {
        ESP_LOGW(TAG, "忽略错误提示更新，界面尚未初始化");
        return;
    }
    ESP_LOGE(TAG, "显示字幕错误提示: %s", text == NULL ? "<placeholder>" : text);
    lvgl_oled_lock();
    lv_obj_set_style_text_color(s_dialog_label_mid, lv_color_hex(UI_TEXT_ERROR_HEX), 0);
    set_caption_line_locked(s_dialog_label_mid, text, "字幕接收异常");
    lvgl_oled_unlock();
}

static void lvgl_task(void *param)
{
    (void)param;
    const TickType_t delay = pdMS_TO_TICKS(LVGL_TASK_DELAY_MS);
    while (true) {
        lvgl_oled_lock();
        lv_timer_handler();
        lvgl_oled_unlock();
        vTaskDelay(delay);
    }
}

static lv_obj_t *create_dialog_label(lv_obj_t *parent, lv_coord_t width, const char *text)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_obj_set_width(label, width);
    lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
    lv_label_set_recolor(label, true);
    lv_obj_set_style_text_font(label, &font_18, 0);
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    lv_obj_set_style_text_line_space(label, 3, 0);
    lv_label_set_text(label, text == NULL ? "" : text);
    if (text == NULL || text[0] == '\0') {
        lv_obj_add_flag(label, LV_OBJ_FLAG_HIDDEN);
    }
    return label;
}

static void create_smart_glasses_demo_ui(void)
{
    lv_obj_t *scr = lv_disp_get_scr_act(s_disp);
    lv_coord_t width = lv_disp_get_hor_res(s_disp);
    lv_coord_t height = lv_disp_get_ver_res(s_disp);

    lv_coord_t visible_top = height / UI_VISIBLE_CROP_TOP_DIV;
    lv_coord_t visible_height = height - visible_top;
    if (visible_height < 60) {
        visible_top = 0;
        visible_height = height;
    }

    lv_coord_t status_width = UI_STATUS_BAR_WIDTH;
    if (status_width > width - 80) {
        status_width = width / 6;
    }
    if (status_width < 20) {
        status_width = 20;
    }

    lv_coord_t divider_width = UI_DIVIDER_WIDTH;
    lv_coord_t main_width = width - status_width - divider_width;
    if (main_width < 60) {
        main_width = width - status_width;
        divider_width = 0;
    }

    lv_coord_t dialog_text_width = main_width - 8;
    if (dialog_text_width < 60) {
        dialog_text_width = main_width;
    }

    /* 左侧状态栏：与可视窗等高，保证图标落在棱镜中心可见区域 */
    s_status_sidebar = lv_obj_create(scr);
    lv_obj_remove_style_all(s_status_sidebar);
    lv_obj_set_size(s_status_sidebar, status_width, visible_height);
    lv_obj_set_pos(s_status_sidebar, 0, visible_top);
    lv_obj_set_style_bg_opa(s_status_sidebar, LV_OPA_TRANSP, 0);
    lv_obj_set_style_pad_all(s_status_sidebar, 0, 0);
    lv_obj_set_flex_flow(s_status_sidebar, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_status_sidebar, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *icon_bt = lv_label_create(s_status_sidebar);
    lv_label_set_text(icon_bt, LV_SYMBOL_BLUETOOTH);
    lv_obj_set_style_text_color(icon_bt, lv_color_hex(0x8AB4F8), 0);

    lv_obj_t *icon_battery = lv_label_create(s_status_sidebar);
    lv_label_set_text(icon_battery, LV_SYMBOL_BATTERY_FULL);
    lv_obj_set_style_text_color(icon_battery, lv_color_white(), 0);

    /* 分割线：只覆盖可视窗高度 */
    if (divider_width > 0) {
        lv_obj_t *divider = lv_obj_create(scr);
        lv_obj_remove_style_all(divider);
        lv_obj_set_size(divider, divider_width, visible_height);
        lv_obj_set_pos(divider, status_width, visible_top);
        lv_obj_set_style_bg_color(divider, lv_color_hex(UI_DIVIDER_COLOR_HEX), 0);
        lv_obj_set_style_bg_opa(divider, LV_OPA_COVER, 0);
    }

    /* 右侧主文本区：限定在中间可视窗，保留定稿 + 实时两行结构 */
    s_main_text_area = lv_obj_create(scr);
    lv_obj_remove_style_all(s_main_text_area);
    lv_obj_set_size(s_main_text_area, main_width, visible_height);
    lv_obj_set_pos(s_main_text_area, status_width + divider_width, visible_top);
    lv_obj_set_style_bg_opa(s_main_text_area, LV_OPA_TRANSP, 0);
    lv_obj_set_style_text_color(s_main_text_area, lv_color_white(), 0);
    lv_obj_set_style_pad_top(s_main_text_area, 4, 0);
    lv_obj_set_style_pad_bottom(s_main_text_area, 4, 0);
    lv_obj_set_style_pad_left(s_main_text_area, 6, 0);
    lv_obj_set_style_pad_right(s_main_text_area, 4, 0);
    lv_obj_set_style_pad_gap(s_main_text_area, UI_DIALOG_GAP, 0);
    lv_obj_set_flex_flow(s_main_text_area, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_main_text_area, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

    s_dialog_label_top = create_dialog_label(s_main_text_area, dialog_text_width, "");
    lv_obj_set_style_text_color(s_dialog_label_top, lv_color_hex(UI_TEXT_COMMITTED_HEX), 0);
    s_dialog_label_mid = create_dialog_label(s_main_text_area, dialog_text_width, "");
    lv_obj_set_style_text_color(s_dialog_label_mid, lv_color_hex(UI_TEXT_LIVE_HEX), 0);
    ESP_LOGI(TAG, "UI布局: %dx%d, 可视窗Y:%d H:%d, 侧边栏:%d, 主区:%d, 文本宽:%d",
             (int)width, (int)height, (int)visible_top, (int)visible_height,
             (int)status_width, (int)main_width, (int)dialog_text_width);
}

static void clear_caption_line_locked(lv_obj_t *label)
{
    if (label == NULL) {
        return;
    }

    lv_label_set_text(label, "");
    lv_obj_add_flag(label, LV_OBJ_FLAG_HIDDEN);
}

static void sanitize_display_text(const char *src, char *dst, size_t dst_len)
{
    static char normalized_buf[UI_TEXT_BUF_LEN];
    size_t di = 0;

    if (dst == NULL || dst_len == 0) {
        return;
    }

    normalize_punctuation_to_ascii(src, normalized_buf, sizeof(normalized_buf));

    for (size_t si = 0; normalized_buf[si] != '\0' && di + 1 < dst_len; ++si) {
        const char ch = normalized_buf[si];
        if (ch == '#') {
            dst[di++] = '/';
            continue;
        }
        if (ch == '\r' || ch == '\n') {
            if (di == 0 || dst[di - 1] == ' ') {
                continue;
            }
            dst[di++] = ' ';
            continue;
        }
        dst[di++] = ch;
    }

    while (di > 0 && dst[di - 1] == ' ') {
        di--;
    }
    dst[di] = '\0';
}

static void set_caption_line_locked(
    lv_obj_t *label,
    const char *text,
    const char *placeholder)
{
    char body_buf[UI_TEXT_BUF_LEN];
    const char *body = placeholder;

    if (label == NULL) {
        return;
    }

    if (text != NULL && text[0] != '\0') {
        sanitize_display_text(text, body_buf, sizeof(body_buf));
        if (body_buf[0] != '\0') {
            body = body_buf;
        }
    }

    if (body == NULL || body[0] == '\0') {
        clear_caption_line_locked(label);
        return;
    }

    ESP_LOGI(TAG, "应用到 LVGL label: %s", body);
    lv_label_set_text(label, body);
    lv_obj_clear_flag(label, LV_OBJ_FLAG_HIDDEN);
}

static void normalize_punctuation_to_ascii(const char *src, char *dst, size_t dst_len)
{
    size_t di = 0;

    if (dst == NULL || dst_len == 0) {
        return;
    }

    if (src == NULL) {
        dst[0] = '\0';
        return;
    }

    while (*src != '\0' && di + 1 < dst_len) {
        const unsigned char c0 = (unsigned char)src[0];
        const unsigned char c1 = (unsigned char)src[1];
        const unsigned char c2 = (unsigned char)src[2];

        /* 常用全角标点转半角，规避字库缺失导致的乱码 */
        if (c0 == 0xEF && c1 != 0 && c2 != 0 && c1 == 0xBC) {
            char repl = '\0';
            if (c2 == 0x8C) {
                repl = ','; /* ， U+FF0C */
            } else if (c2 == 0x81) {
                repl = '!'; /* ！ U+FF01 */
            } else if (c2 == 0x9F) {
                repl = '?'; /* ？ U+FF1F */
            } else if (c2 == 0x9A) {
                repl = ':'; /* ： U+FF1A */
            } else if (c2 == 0x9B) {
                repl = ';'; /* ； U+FF1B */
            }

            if (repl != '\0') {
                dst[di++] = repl;
                src += 3;
                continue;
            }
        }

        if (c0 == 0xE3 && c1 != 0 && c2 != 0 && c1 == 0x80) {
            if (c2 == 0x82) {
                dst[di++] = '.'; /* 。 U+3002 */
                src += 3;
                continue;
            }

            if (c2 == 0x81) {
                dst[di++] = ','; /* 、 U+3001 */
                src += 3;
                continue;
            }
        }

        dst[di++] = *src++;
    }

    dst[di] = '\0';
}

