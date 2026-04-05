| Supported Targets | ESP32-S3 |
| ----------------- | -------- |

# ARnav ESP32

这是一个运行在 ESP32-S3 上的导航显示项目。

它会把导航信息显示在 ST7789 屏幕上，适合做：

- AR 导航副屏
- 车载 HUD 小屏
- 导航状态显示终端

项目当前使用 BLE 输入：

- BLE：由手机或上位机直接发送文本字符串

如果你只是想把屏幕跑起来并显示导航信息，优先使用 BLE，配置最简单。

## 你需要准备什么

- ESP32-S3 开发板
- ST7789 屏幕，分辨率 `172x320`
- USB 数据线
- ESP-IDF 5.2 环境

默认屏幕引脚定义在 `main/LCD_Driver/ST7789.h`，如果你的接线不同，需要先改引脚再编译。

## 快速开始

### 1. 导出 ESP-IDF 环境

先按 ESP-IDF 官方方式安装好 ESP-IDF 5.2，然后在项目目录里执行：

```bash
source <你的 ESP-IDF 路径>/export.sh
```

### 2. （可选）创建本地 Wi-Fi 配置文件

当前项目核心功能只依赖 BLE，上屏不依赖 micro-ROS。

如果你仍需要项目启动时尝试连接 Wi-Fi，可创建本地配置文件：

```bash
cp main/project_local_config.private.example.h main/project_local_config.private.h
```

然后填写：

```c
#define PROJECT_WIFI_SSID "your_wifi_ssid"
#define PROJECT_WIFI_PASSWORD "your_wifi_password"
#define PROJECT_WIFI_MAXIMUM_RETRY 5
```

### 3. 编译并烧录

```bash
idf.py set-target esp32s3
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

退出串口监视器：

```bash
Ctrl+]
```

## 屏幕方向

如果你使用的是镜像显示结构，可以在 `main/LVGL_Driver/LVGL_Driver.h` 中调整：

```c
#define LVGL_MIRROR_HORIZONTAL 1
#define LVGL_MIRROR_VERTICAL 0
```

如果你是普通直视屏，发现画面左右或上下方向不对，就改这里。

## 如何给它发送蓝牙字符串

### 方式一：BLE

设备启动后会作为 BLE GATT Server 工作。

- 服务 UUID：`0x00FF`
- 特征 UUID：`0xFF01`

向特征写入文本数据即可，设备会把收到的字符串直接显示在屏幕上。

示例：

```text
Hello ARnav
```

说明：

- 不再要求 `N1/N2/D` 协议
- 不再使用导航图标和车辆图标

## 常见问题

### 屏幕不亮

- 检查供电
- 检查背光引脚
- 检查 `ST7789.h` 里的引脚配置是否和你的接线一致

### 画面方向不对

- 修改 `main/LVGL_Driver/LVGL_Driver.h` 中的镜像设置

### BLE 连上了但没有显示内容

- 确认写入的是 `0xFF01`
- 确认发送的是可显示文本（UTF-8）
- 确认单包长度不要超过当前 MTU 限制

### 编译失败

- 确认已经正确导出 ESP-IDF 环境
- 确认当前终端没有被错误的 Python 环境干扰

## 你通常只需要改这几个地方

- `main/project_local_config.private.h`：本地 Wi-Fi 配置（可选）
- `main/LCD_Driver/ST7789.h`：屏幕引脚
- `main/LVGL_Driver/LVGL_Driver.h`：画面镜像方向


