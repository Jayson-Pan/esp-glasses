/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#ifndef WIFI_CONNECT_H
#define WIFI_CONNECT_H

#include "esp_err.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize WiFi and connect to configured AP
 * 
 * @return esp_err_t ESP_OK on success
 */
esp_err_t wifi_connect_init(void);

/**
 * @brief Disconnect WiFi and deinitialize
 * 
 * @return esp_err_t ESP_OK on success
 */
esp_err_t wifi_connect_deinit(void);

/**
 * @brief Check if WiFi is connected
 * 
 * @return true if WiFi is connected, false otherwise
 */
bool is_wifi_connected(void);

#ifdef __cplusplus
}
#endif

#endif /* WIFI_CONNECT_H */
