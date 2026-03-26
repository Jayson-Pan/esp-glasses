/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#ifndef MICRO_ROS_SUB_H
#define MICRO_ROS_SUB_H

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 启动 micro-ROS 订阅节点
 * 
 * 创建一个订阅 "topic" 话题的节点，用于接收来自
 * ros2 run examples_rclcpp_minimal_publisher publisher_member_function
 * 发布的消息
 * 
 * @return ESP_OK 成功, ESP_FAIL 失败
 */
esp_err_t micro_ros_subscriber_start(void);

/**
 * @brief 停止 micro-ROS 订阅节点
 */
void micro_ros_subscriber_stop(void);

#ifdef __cplusplus
}
#endif

#endif /* MICRO_ROS_SUB_H */
