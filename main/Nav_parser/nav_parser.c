/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include "nav_parser.h"
#include <string.h>
#include <stdlib.h>
#include "esp_log.h"

static const char *TAG = "NAV_PARSER";

/**
 * @brief 解析N1类型的文本数据
 * 格式: N1|当前道路名|转向码\n
 */
static bool parse_n1_text(const char *text, nav_packet_n1_t *n1) {
    if (!text || !n1) {
        return false;
    }

    // 复制字符串用于 strtok 处理
    char buffer[128];
    strncpy(buffer, text, sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';
    
    // 移除末尾的换行符
    size_t len = strlen(buffer);
    if (len > 0 && (buffer[len-1] == '\n' || buffer[len-1] == '\r')) {
        buffer[len-1] = '\0';
        if (len > 1 && buffer[len-2] == '\r') {
            buffer[len-2] = '\0';
        }
    }

    // 分割字符串: N1|当前道路名|转向码
    char *token = strtok(buffer, "|");
    if (!token || strcmp(token, "N1") != 0) {
        ESP_LOGE(TAG, "N1包格式错误: 缺少N1标识");
        return false;
    }

    // 获取当前道路名
    token = strtok(NULL, "|");
    if (!token) {
        ESP_LOGE(TAG, "N1包格式错误: 缺少当前道路名");
        return false;
    }
    strncpy(n1->current_road, token, sizeof(n1->current_road) - 1);
    n1->current_road[sizeof(n1->current_road) - 1] = '\0';

    // 获取转向码
    token = strtok(NULL, "|");
    if (!token) {
        ESP_LOGE(TAG, "N1包格式错误: 缺少转向码");
        return false;
    }
    n1->icon_type = atoi(token);

    return true;
}

/**
 * @brief 解析N2类型的文本数据
 * 格式: N2|下一道路名\n
 */
static bool parse_n2_text(const char *text, nav_packet_n2_t *n2) {
    if (!text || !n2) {
        return false;
    }

    // 复制字符串用于 strtok 处理
    char buffer[128];
    strncpy(buffer, text, sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';
    
    // 移除末尾的换行符
    size_t len = strlen(buffer);
    if (len > 0 && (buffer[len-1] == '\n' || buffer[len-1] == '\r')) {
        buffer[len-1] = '\0';
        if (len > 1 && buffer[len-2] == '\r') {
            buffer[len-2] = '\0';
        }
    }

    // 分割字符串: N2|下一道路名
    char *token = strtok(buffer, "|");
    if (!token || strcmp(token, "N2") != 0) {
        ESP_LOGE(TAG, "N2包格式错误: 缺少N2标识");
        return false;
    }

    // 获取下一道路名
    token = strtok(NULL, "|");
    if (!token) {
        ESP_LOGE(TAG, "N2包格式错误: 缺少下一道路名");
        return false;
    }
    strncpy(n2->next_road, token, sizeof(n2->next_road) - 1);
    n2->next_road[sizeof(n2->next_road) - 1] = '\0';

    return true;
}

/**
 * @brief 解析D类型的文本数据
 * 格式: D|距离下一路口|距离终点\n
 */
static bool parse_d_text(const char *text, nav_packet_d_t *d) {
    if (!text || !d) {
        return false;
    }

    // 复制字符串用于 strtok 处理
    char buffer[64];
    strncpy(buffer, text, sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';
    
    // 移除末尾的换行符
    size_t len = strlen(buffer);
    if (len > 0 && (buffer[len-1] == '\n' || buffer[len-1] == '\r')) {
        buffer[len-1] = '\0';
        if (len > 1 && buffer[len-2] == '\r') {
            buffer[len-2] = '\0';
        }
    }

    // 分割字符串: D|距离下一路口|距离终点
    char *token = strtok(buffer, "|");
    if (!token || strcmp(token, "D") != 0) {
        ESP_LOGE(TAG, "D包格式错误: 缺少D标识");
        return false;
    }

    // 获取距离下一路口
    token = strtok(NULL, "|");
    if (!token) {
        ESP_LOGE(TAG, "D包格式错误: 缺少距离下一路口");
        return false;
    }
    d->distance_to_next = atoi(token);

    // 获取距离终点
    token = strtok(NULL, "|");
    if (!token) {
        ESP_LOGE(TAG, "D包格式错误: 缺少距离终点");
        return false;
    }
    d->distance_to_dest = atoi(token);

    return true;
}

bool nav_parser_parse_text(const char *text_str, nav_packet_t *packet) {
    if (!text_str || !packet) {
        ESP_LOGE(TAG, "输入参数为NULL");
        return false;
    }

    // 初始化packet
    memset(packet, 0, sizeof(nav_packet_t));
    packet->type = NAV_PACKET_TYPE_UNKNOWN;

    // 判断数据包类型
    if (strncmp(text_str, "N1|", 3) == 0) {
        packet->type = NAV_PACKET_TYPE_N1;
        if (parse_n1_text(text_str, &packet->data.n1)) {
            ESP_LOGI(TAG, "N1数据包解析成功");
            return true;
        } else {
            ESP_LOGE(TAG, "N1数据包解析失败");
            return false;
        }
    } 
    else if (strncmp(text_str, "N2|", 3) == 0) {
        packet->type = NAV_PACKET_TYPE_N2;
        if (parse_n2_text(text_str, &packet->data.n2)) {
            ESP_LOGI(TAG, "N2数据包解析成功");
            return true;
        } else {
            ESP_LOGE(TAG, "N2数据包解析失败");
            return false;
        }
    } 
    else if (strncmp(text_str, "D|", 2) == 0) {
        packet->type = NAV_PACKET_TYPE_D;
        if (parse_d_text(text_str, &packet->data.d)) {
            ESP_LOGI(TAG, "D数据包解析成功");
            return true;
        } else {
            ESP_LOGE(TAG, "D数据包解析失败");
            return false;
        }
    } 
    else {
        ESP_LOGE(TAG, "未知的数据包类型: %.10s", text_str);
        return false;
    }
}

void nav_parser_print_n1(const nav_packet_n1_t *n1) {
    if (!n1) {
        ESP_LOGE(TAG, "N1数据指针为NULL");
        return;
    }

    ESP_LOGI(TAG, "========== N1 当前道路信息 ==========");
    ESP_LOGI(TAG, "当前道路: %s", n1->current_road);
    ESP_LOGI(TAG, "转向类型: %d", n1->icon_type);
    ESP_LOGI(TAG, "====================================");
}

void nav_parser_print_n2(const nav_packet_n2_t *n2) {
    if (!n2) {
        ESP_LOGE(TAG, "N2数据指针为NULL");
        return;
    }

    ESP_LOGI(TAG, "========== N2 下一道路名 ==========");
    ESP_LOGI(TAG, "下一道路: %s", n2->next_road);
    ESP_LOGI(TAG, "===================================");
}

void nav_parser_print_d(const nav_packet_d_t *d) {
    if (!d) {
        ESP_LOGE(TAG, "D数据指针为NULL");
        return;
    }

    ESP_LOGI(TAG, "========== D 距离信息 ==========");
    ESP_LOGI(TAG, "到下一路口: %d 米", d->distance_to_next);
    ESP_LOGI(TAG, "到终点: %d 米", d->distance_to_dest);
    ESP_LOGI(TAG, "================================");
}

void nav_parser_print_packet(const nav_packet_t *packet) {
    if (!packet) {
        ESP_LOGE(TAG, "数据包指针为NULL");
        return;
    }

    switch (packet->type) {
        case NAV_PACKET_TYPE_N1:
            nav_parser_print_n1(&packet->data.n1);
            break;
        
        case NAV_PACKET_TYPE_N2:
            nav_parser_print_n2(&packet->data.n2);
            break;
        
        case NAV_PACKET_TYPE_D:
            nav_parser_print_d(&packet->data.d);
            break;
        
        case NAV_PACKET_TYPE_UNKNOWN:
        default:
            ESP_LOGE(TAG, "未知的数据包类型: %d", packet->type);
            break;
    }
}
