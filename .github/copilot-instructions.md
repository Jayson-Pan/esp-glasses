# Project Guidelines

## Build and Test
- This project targets ESP-IDF 5.2 on ESP32-S3.
- Before any build commands, load the ESP-IDF environment: source $IDF_PATH/export.sh.
- Preferred verification command for code changes: idf.py build.
- Hardware workflow (only when explicitly requested): idf.py -p <PORT> flash monitor.
- Set target to esp32s3 when initializing a fresh build directory: idf.py set-target esp32s3.
- There is no host-side automated unit test suite in this repository; use build success plus hardware smoke checks when needed.

## Architecture
- Firmware code is organized under main/ as one ESP-IDF component with feature folders:
  - UI and rendering: main/LVGL_UI, main/LVGL_Driver, main/LCD_Driver.
  - Connectivity: main/BLE_server, main/Wireless.
  - Sensors and device drivers: main/QMI8658, main/I2C_Driver, main/BAT_Driver.
  - Navigation parsing: main/Nav_parser.
- Component registration is centralized in main/CMakeLists.txt; new source modules must be added there.

## Conventions
- Keep local credentials and network settings in main/project_local_config.private.h (copy from main/project_local_config.private.example.h). Never commit secrets.
- Configuration precedence is intentional: main/project_local_config.h first includes private overrides, then falls back to sdkconfig values.
- If you add new feature files, update both SRCS and INCLUDE_DIRS in main/CMakeLists.txt.
- Screen wiring and orientation are configured in main/LCD_Driver/ST7789.h and main/LVGL_Driver/LVGL_Driver.h; preserve existing board assumptions unless the task asks to change hardware mapping.
- BLE behavior is text-display oriented: received BLE strings are shown directly on screen.

## Documentation
- Use README.md as the source of truth for setup, flashing, and BLE payload behavior.
- This repository currently has no dedicated docs/ tree; avoid duplicating README content in new instruction files.
