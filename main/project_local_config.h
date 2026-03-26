#ifndef PROJECT_LOCAL_CONFIG_H
#define PROJECT_LOCAL_CONFIG_H

/*
 * Local private config takes precedence over sdkconfig values.
 * Keep secrets and local network settings in project_local_config.private.h,
 * which is ignored by Git.
 */
#if defined(__has_include)
#if __has_include("project_local_config.private.h")
#include "project_local_config.private.h"
#endif
#endif

#ifndef PROJECT_WIFI_SSID
#ifdef CONFIG_ESP_WIFI_SSID
#define PROJECT_WIFI_SSID CONFIG_ESP_WIFI_SSID
#else
#define PROJECT_WIFI_SSID ""
#endif
#endif

#ifndef PROJECT_WIFI_PASSWORD
#ifdef CONFIG_ESP_WIFI_PASSWORD
#define PROJECT_WIFI_PASSWORD CONFIG_ESP_WIFI_PASSWORD
#else
#define PROJECT_WIFI_PASSWORD ""
#endif
#endif

#ifndef PROJECT_WIFI_MAXIMUM_RETRY
#ifdef CONFIG_ESP_MAXIMUM_RETRY
#define PROJECT_WIFI_MAXIMUM_RETRY CONFIG_ESP_MAXIMUM_RETRY
#else
#define PROJECT_WIFI_MAXIMUM_RETRY 5
#endif
#endif

#ifndef PROJECT_MICRO_ROS_AGENT_IP
#ifdef CONFIG_MICRO_ROS_AGENT_IP
#define PROJECT_MICRO_ROS_AGENT_IP CONFIG_MICRO_ROS_AGENT_IP
#else
#define PROJECT_MICRO_ROS_AGENT_IP ""
#endif
#endif

#ifndef PROJECT_MICRO_ROS_AGENT_PORT
#ifdef CONFIG_MICRO_ROS_AGENT_PORT
#define PROJECT_MICRO_ROS_AGENT_PORT CONFIG_MICRO_ROS_AGENT_PORT
#else
#define PROJECT_MICRO_ROS_AGENT_PORT "8888"
#endif
#endif

#endif /* PROJECT_LOCAL_CONFIG_H */
