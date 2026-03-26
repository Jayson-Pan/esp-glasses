#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"

// 字体声明
LV_FONT_DECLARE(vehicle);

// 图标符号定义 (UTF-8编码)
#define TRUCK         "\xEE\x9A\xB0"  
#define CAR           "\xEE\x98\xBD"  
#define EBIKE         "\xEE\xAC\x99"  
#define MOTORCYCLE    "\xEE\x98\x86"  
#define BICYCLE       "\xEE\x98\xAA"  


#ifdef __cplusplus
}
#endif