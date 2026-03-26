#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"

// 字体声明
LV_FONT_DECLARE(nav_icon);

// 图标符号定义 (UTF-8编码)
#define ICON_FORWARD        "\xEE\xA0\x80"  // U+E800
#define ICON_LEFT           "\xEE\xA0\x81"  // U+E801
#define ICON_RIGHT          "\xEE\xA0\x82"  // U+E802
#define ICON_FRONT_LEFT     "\xEE\xA0\x83"  // U+E803
#define ICON_FRONT_RIGHT    "\xEE\xA0\x84"  // U+E804
#define ICON_REAR_LEFT      "\xEE\xA0\x85"  // U+E805
#define ICON_REAR_RIGHT     "\xEE\xA0\x86"  // U+E806
#define ICON_END            "\xEE\xA0\x87"  // U+E807

#ifdef __cplusplus
}
#endif
