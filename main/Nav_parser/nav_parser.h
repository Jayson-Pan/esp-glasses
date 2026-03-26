/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#ifndef NAV_PARSER_H
#define NAV_PARSER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 数据包类型枚举
 */
typedef enum {
    NAV_PACKET_TYPE_UNKNOWN = 0,  // 未知类型
    NAV_PACKET_TYPE_N1 = 1,       // N1: 当前道路信息
    NAV_PACKET_TYPE_N2 = 2,       // N2: 下一道路名
    NAV_PACKET_TYPE_D = 3,        // D: 距离信息
} nav_packet_type_t;

/**
 * @brief N1 数据包结构 - 当前道路信息
 * 格式: N1|当前道路名|转向码\n
 */
typedef struct {
    char current_road[64];        // 当前所在道路名
    int icon_type;                // 转向类型编号(0-66)
} nav_packet_n1_t;

/**
 * @brief N2 数据包结构 - 下一道路名
 * 格式: N2|下一道路名\n
 */
typedef struct {
    char next_road[64];           // 下一道路名
} nav_packet_n2_t;

/**
 * @brief D 数据包结构 - 距离信息
 * 格式: D|距离下一路口|距离终点\n
 */
typedef struct {
    int distance_to_next;         // 到下一路口的距离(米)
    int distance_to_dest;         // 到终点的距离(米)
} nav_packet_d_t;

/**
 * @brief 导航数据包联合体
 */
typedef struct {
    nav_packet_type_t type;       // 数据包类型
    union {
        nav_packet_n1_t n1;       // N1数据
        nav_packet_n2_t n2;       // N2数据
        nav_packet_d_t d;         // D数据
    } data;
} nav_packet_t;

/**
 * @brief 解析文本格式的导航数据包
 * 支持三种格式:
 * - N1|当前道路名|转向码\n
 * - N2|下一道路名\n
 * - D|距离下一路口|距离终点\n
 * 
 * @param text_str 文本字符串
 * @param packet 输出的解析结果
 * @return true 解析成功
 * @return false 解析失败
 */
bool nav_parser_parse_text(const char *text_str, nav_packet_t *packet);

/**
 * @brief 打印N1数据包内容到日志
 * 
 * @param n1 N1数据包指针
 */
void nav_parser_print_n1(const nav_packet_n1_t *n1);

/**
 * @brief 打印N2数据包内容到日志
 * 
 * @param n2 N2数据包指针
 */
void nav_parser_print_n2(const nav_packet_n2_t *n2);

/**
 * @brief 打印D数据包内容到日志
 * 
 * @param d D数据包指针
 */
void nav_parser_print_d(const nav_packet_d_t *d);

/**
 * @brief 打印完整导航数据包到日志
 * 
 * @param packet 导航数据包指针
 */
void nav_parser_print_packet(const nav_packet_t *packet);

#ifdef __cplusplus
}
#endif

#endif /* NAV_PARSER_H */
