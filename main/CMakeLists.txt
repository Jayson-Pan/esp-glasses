idf_component_register(SRCS "Nav_parser/nav_parser.c"
                           "LVGL_UI/LVGL_Example.c"
                           "LVGL_Driver/LVGL_Driver.c"
                           "LCD_Driver/ST7789.c"
                           "LCD_Driver/Vernon_ST7789T/Vernon_ST7789T.c"
                           "BLE_server/gatts_demo.c"
                           "Wireless/wifi_connect.c"
                           "Microros/micro_ros_sub.c"
                           "BAT_Driver/BAT_Driver.c"
                           "I2C_Driver/I2C_Driver.c"
                           "QMI8658/QMI8658.c"
                           "fonts/font_20.c"
                           "fonts/nav_icon.c"
                           "fonts/vehicle.c"
                    PRIV_REQUIRES bt nvs_flash json esp_wifi esp_netif micro_ros_espidf_component esp_lcd esp_adc driver
                    INCLUDE_DIRS "." "Nav_parser" "LVGL_UI" "LVGL_Driver" "LCD_Driver" "LCD_Driver/Vernon_ST7789T" "BLE_server" "Wireless" "Microros" "BAT_Driver" "I2C_Driver" "QMI8658" "fonts")

# 抑制来自 ESP-IDF 框架的 UHCI 相关提示信息
target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-cpp)
