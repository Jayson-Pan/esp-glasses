#include "LVGL_Driver.h"

static const char *TAG_LVGL = "WS_LVGL";

static lv_color_t buf1[ LVGL_BUF_LEN ];
static lv_color_t buf2[ LVGL_BUF_LEN];
// static lv_color_t* buf1 = (lv_color_t*) heap_caps_malloc(LVGL_BUF_LEN , MALLOC_CAP_SPIRAM);
// static lv_color_t* buf2 = (lv_color_t*) heap_caps_malloc(LVGL_BUF_LEN , MALLOC_CAP_SPIRAM);
    

lv_disp_draw_buf_t disp_buf;                                                 // contains internal graphic buffer(s) called draw buffer(s)
lv_disp_drv_t disp_drv;                                                      // contains callback functions
    
lv_indev_drv_t indev_drv;
esp_timer_handle_t lvgl_tick_timer = NULL;

void example_increase_lvgl_tick(void *arg)
{
    /* Tell LVGL how many milliseconds has elapsed */
    lv_tick_inc(EXAMPLE_LVGL_TICK_PERIOD_MS);
}

bool example_notify_lvgl_flush_ready(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx)
{
    lv_disp_drv_t *disp_driver = (lv_disp_drv_t *)user_ctx;
    lv_disp_flush_ready(disp_driver);
    return false;
}

void example_lvgl_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map)
{
    esp_lcd_panel_handle_t panel_handle = (esp_lcd_panel_handle_t) drv->user_data;
    lv_coord_t width = area->x2 - area->x1 + 1;
    lv_coord_t height = area->y2 - area->y1 + 1;
    lv_coord_t dest_x1 = area->x1;
    lv_coord_t dest_x2 = area->x2;
    lv_coord_t dest_y1 = area->y1;
    lv_coord_t dest_y2 = area->y2;

#if LVGL_MIRROR_HORIZONTAL
    for (lv_coord_t row = 0; row < height; row++) {
        lv_color_t *row_ptr = color_map + row * width;
        for (lv_coord_t col = 0; col < width / 2; col++) {
            lv_color_t tmp = row_ptr[col];
            row_ptr[col] = row_ptr[width - 1 - col];
            row_ptr[width - 1 - col] = tmp;
        }
    }
    dest_x1 = drv->hor_res - 1 - area->x2;
    dest_x2 = drv->hor_res - 1 - area->x1;
#endif

#if LVGL_MIRROR_VERTICAL
    for (lv_coord_t row = 0; row < height / 2; row++) {
        lv_color_t *row_top = color_map + row * width;
        lv_color_t *row_bottom = color_map + (height - 1 - row) * width;
        for (lv_coord_t col = 0; col < width; col++) {
            lv_color_t tmp = row_top[col];
            row_top[col] = row_bottom[col];
            row_bottom[col] = tmp;
        }
    }
    dest_y1 = drv->ver_res - 1 - area->y2;
    dest_y2 = drv->ver_res - 1 - area->y1;
#endif

    int panel_x1 = dest_x1 + Offset_X;
    int panel_x2 = dest_x2 + Offset_X + 1;
    int panel_y1 = dest_y1 + Offset_Y;
    int panel_y2 = dest_y2 + Offset_Y + 1;

    // 复制缓冲区内容到显示屏的指定区域
    esp_lcd_panel_draw_bitmap(panel_handle, panel_x1, panel_y1, panel_x2, panel_y2, color_map);
}

/*Read the touchpad*/
void example_touchpad_read(lv_indev_drv_t *drv, lv_indev_data_t *data)
{
    (void)drv;
    data->point.x = 0;
    data->point.y = 0;
    data->state = LV_INDEV_STATE_REL;
}
/* Rotate display and touch, when rotated screen in LVGL. Called when driver parameters are updated. */
void example_lvgl_port_update_callback(lv_disp_drv_t *drv)
{
    esp_lcd_panel_handle_t panel_handle = (esp_lcd_panel_handle_t) drv->user_data;

    switch (drv->rotated) {
    case LV_DISP_ROT_NONE:
        // Rotate LCD display
        esp_lcd_panel_swap_xy(panel_handle, false);
        esp_lcd_panel_mirror(panel_handle, true, false);
        break;
    case LV_DISP_ROT_90:
        // Rotate LCD display
        esp_lcd_panel_swap_xy(panel_handle, true);
        esp_lcd_panel_mirror(panel_handle, true, true);
        break;
    case LV_DISP_ROT_180:
        // Rotate LCD display
        esp_lcd_panel_swap_xy(panel_handle, false);
        esp_lcd_panel_mirror(panel_handle, false, true);
        break;
    case LV_DISP_ROT_270:
        // Rotate LCD display
        esp_lcd_panel_swap_xy(panel_handle, true);
        esp_lcd_panel_mirror(panel_handle, false, false);
        break;
    }
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
lv_disp_t *disp;
void LVGL_Init(void)
{
    ESP_LOGI(TAG_LVGL, "Initialize LVGL library");
    lv_init();
    ESP_LOGI(TAG_LVGL, "Display mirror config -> horizontal:%d vertical:%d", LVGL_MIRROR_HORIZONTAL, LVGL_MIRROR_VERTICAL);
    
    lv_disp_draw_buf_init(&disp_buf, buf1, buf2, LVGL_BUF_LEN);                              // initialize LVGL draw buffers

    ESP_LOGI(TAG_LVGL, "Register display driver to LVGL");
    lv_disp_drv_init(&disp_drv);                                                                        // Create a new screen object and initialize the associated device
    disp_drv.hor_res = EXAMPLE_LCD_H_RES;             
    disp_drv.ver_res = EXAMPLE_LCD_V_RES;                                                     // Horizontal pixel count
    disp_drv.rotated = LV_DISP_ROT_90; // 图像旋转                                                            // Vertical axis pixel count
    disp_drv.sw_rotate = true;
    disp_drv.flush_cb = example_lvgl_flush_cb;                                                          // Function : copy a buffer's content to a specific area of the display
    disp_drv.drv_update_cb = example_lvgl_port_update_callback;                                         // Function : Rotate display and touch, when rotated screen in LVGL. Called when driver parameters are updated. 
    disp_drv.draw_buf = &disp_buf;                                                                      // LVGL will use this buffer(s) to draw the screens contents
    disp_drv.user_data = panel_handle;                
    ESP_LOGI(TAG_LVGL,"Register display indev to LVGL");                                                  // Custom display driver user data
    disp = lv_disp_drv_register(&disp_drv);                                                  // Create screen objects
    
    /********************* LVGL *********************/
    ESP_LOGI(TAG_LVGL, "Install LVGL tick timer");
    // Tick interface for LVGL (using esp_timer to generate 2ms periodic event)
    const esp_timer_create_args_t lvgl_tick_timer_args = {
        .callback = &example_increase_lvgl_tick,
        .name = "lvgl_tick"
    };

    /********************* LVGL *********************/
    ESP_LOGI(TAG_LVGL,"Register display indev to LVGL");
    lv_indev_drv_init ( &indev_drv );
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.disp = disp;
    indev_drv.read_cb = example_touchpad_read;
    lv_indev_drv_register( &indev_drv );
    
    ESP_ERROR_CHECK(esp_timer_create(&lvgl_tick_timer_args, &lvgl_tick_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(lvgl_tick_timer, EXAMPLE_LVGL_TICK_PERIOD_MS * 1000));

}
