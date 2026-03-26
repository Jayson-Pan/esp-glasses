/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"

#include <uros_network_interfaces.h>
#include <rcl/rcl.h>
#include <rcl/error_handling.h>
#include <std_msgs/msg/bool.h>
#include <rclc/rclc.h>
#include <rclc/executor.h>

#ifdef CONFIG_MICRO_ROS_ESP_XRCE_DDS_MIDDLEWARE
#include <rmw_microros/rmw_microros.h>
#endif

#include "project_local_config.h"
#include "micro_ros_sub.h"

static const char *TAG = "MICRO_ROS_SUB";

/* 来自 UI 的车辆显示接口 */
extern void lvgl_set_vehicle_detect(bool detected);
// micro-ROS 对象
static rcl_allocator_t allocator;
static rclc_support_t support;
static rcl_node_t node;
static rcl_subscription_t subscriber;
static rclc_executor_t executor;

// 订阅的消息（Bool）
static std_msgs__msg__Bool recv_msg;

// micro-ROS 状态标志
static bool microros_initialized = false;

#define RCCHECK(fn) { rcl_ret_t temp_rc = fn; if((temp_rc != RCL_RET_OK)){ESP_LOGE(TAG, "Failed status on line %d: %d. Aborting.", __LINE__,(int)temp_rc); return false;}}
#define RCSOFTCHECK(fn) { rcl_ret_t temp_rc = fn; if((temp_rc != RCL_RET_OK)){ESP_LOGW(TAG, "Failed status on line %d: %d. Continuing.", __LINE__,(int)temp_rc);}}

/**
 * @brief 订阅回调函数
 * 当接收到话题消息时被调用
 */
static void subscription_callback(const void *msgin)
{
    const std_msgs__msg__Bool *msg = (const std_msgs__msg__Bool *)msgin;
    ESP_LOGI(TAG, "Received Bool: %s", msg->data ? "true" : "false");
    lvgl_set_vehicle_detect(msg->data);
}

/**
 * @brief 初始化 micro-ROS
 */
static bool microros_init(void)
{
    // 初始化 allocator
    allocator = rcl_get_default_allocator();

    // 创建 init_options
    rcl_init_options_t init_options = rcl_get_zero_initialized_init_options();
    RCCHECK(rcl_init_options_init(&init_options, allocator));

#ifdef CONFIG_MICRO_ROS_ESP_XRCE_DDS_MIDDLEWARE
    rmw_init_options_t* rmw_options = rcl_init_options_get_rmw_init_options(&init_options);

    if (PROJECT_MICRO_ROS_AGENT_IP[0] == '\0') {
        ESP_LOGE(TAG, "micro-ROS Agent IP is not configured. Create main/project_local_config.private.h or set the ESP-IDF micro-ROS options locally.");
        RCSOFTCHECK(rcl_init_options_fini(&init_options));
        return false;
    }

    RCCHECK(rmw_uros_options_set_udp_address(PROJECT_MICRO_ROS_AGENT_IP, PROJECT_MICRO_ROS_AGENT_PORT, rmw_options));
#endif

    // 使用 init_options 初始化 support
    RCCHECK(rclc_support_init_with_options(&support, 0, NULL, &init_options, &allocator));
    RCSOFTCHECK(rcl_init_options_fini(&init_options));

    // 创建节点
    node = rcl_get_zero_initialized_node();
    RCCHECK(rclc_node_init_default(&node, "esp32_subscriber_node", "", &support));

    // 创建订阅者 - 订阅 "/vehicle_detect" 话题
    RCCHECK(rclc_subscription_init_default(
        &subscriber,
        &node,
        ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, Bool),
        "/vehicle_detect"));

    // 初始化接收消息
    recv_msg.data = false;

    // 创建执行器
    executor = rclc_executor_get_zero_initialized_executor();
    RCCHECK(rclc_executor_init(&executor, &support.context, 1, &allocator));
    RCCHECK(rclc_executor_add_subscription(&executor, &subscriber, &recv_msg, &subscription_callback, ON_NEW_DATA));

    ESP_LOGI(TAG, "micro-ROS subscriber initialized successfully");
    microros_initialized = true;
    return true;
}

/**
 * @brief 清理 micro-ROS 资源
 */
static void microros_cleanup(void)
{
    if (!microros_initialized) {
        return;
    }

    RCSOFTCHECK(rcl_subscription_fini(&subscriber, &node));
    RCSOFTCHECK(rcl_node_fini(&node));
    RCSOFTCHECK(rclc_executor_fini(&executor));
    RCSOFTCHECK(rclc_support_fini(&support));

    microros_initialized = false;
    ESP_LOGI(TAG, "micro-ROS cleaned up");
}

/**
 * @brief micro-ROS 任务
 */
static void micro_ros_task(void *arg)
{
    ESP_LOGI(TAG, "micro-ROS task started");

#if defined(CONFIG_MICRO_ROS_ESP_NETIF_WLAN) || defined(CONFIG_MICRO_ROS_ESP_NETIF_ENET)
    // 等待网络连接
    ESP_LOGI(TAG, "Waiting for network connection...");
    extern bool is_wifi_connected(void);
    while (!is_wifi_connected()) {
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    ESP_LOGI(TAG, "Network connected, starting micro-ROS");
    
    // 给网络一点时间稳定
    vTaskDelay(pdMS_TO_TICKS(2000));
#endif

    // 初始化 micro-ROS
    if (!microros_init()) {
        ESP_LOGE(TAG, "Failed to initialize micro-ROS");
        vTaskDelete(NULL);
        return;
    }

    // 主循环 - 处理订阅消息
    while (microros_initialized) {
        // 执行器会调用订阅回调函数
        rclc_executor_spin_some(&executor, RCL_MS_TO_NS(100));
        usleep(100000);  // 100ms
    }

    // 清理
    microros_cleanup();
    vTaskDelete(NULL);
}

/**
 * @brief 启动 micro-ROS 订阅节点
 */
esp_err_t micro_ros_subscriber_start(void)
{
    ESP_LOGI(TAG, "Starting micro-ROS subscriber");

    // 创建 micro-ROS 任务
    // 栈大小：16KB，优先级：5
    BaseType_t ret = xTaskCreate(
        micro_ros_task,
        "micro_ros_task",
        16384,  // 栈大小 16KB
        NULL,
        5,      // 任务优先级
        NULL
    );

    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create micro-ROS task");
        return ESP_FAIL;
    }

    return ESP_OK;
}

/**
 * @brief 停止 micro-ROS 订阅节点
 */
void micro_ros_subscriber_stop(void)
{
    ESP_LOGI(TAG, "Stopping micro-ROS subscriber");
    microros_initialized = false;
}
