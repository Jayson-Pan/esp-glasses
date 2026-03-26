| Supported Targets | ESP32-S3 |
| ----------------- | -------- |

# ARnav ESP32

这是一个运行在 ESP32-S3 上的导航显示项目。

它会把导航信息显示在 ST7789 屏幕上，适合做：

- AR 导航副屏
- 车载 HUD 小屏
- 导航状态显示终端

项目支持两种输入方式：

- BLE：由手机或上位机直接发送导航数据
- micro-ROS：通过 Wi-Fi 接入 ROS 2 网络

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

### 2. 创建本地配置文件

这个项目把本地 Wi-Fi 和 micro-ROS 配置放在：

```bash
cp main/project_local_config.private.example.h main/project_local_config.private.h
```

打开 `main/project_local_config.private.h`，按需要填写：

```c
#define PROJECT_WIFI_SSID "your_wifi_ssid"
#define PROJECT_WIFI_PASSWORD "your_wifi_password"
#define PROJECT_WIFI_MAXIMUM_RETRY 5

#define PROJECT_MICRO_ROS_AGENT_IP "192.168.1.100"
#define PROJECT_MICRO_ROS_AGENT_PORT "8888"
```

说明：

- 只使用 BLE 时，可以先不配置 micro-ROS Agent
- 使用 micro-ROS 时，需要填写 Wi-Fi 和 Agent 地址

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

## 如何给它发送导航数据

### 方式一：BLE

设备启动后会作为 BLE GATT Server 工作。

- 服务 UUID：`0x00FF`
- 特征 UUID：`0xFF01`

向特征写入文本数据，每条数据必须以换行符 `\n` 结尾。

支持 3 种消息：

#### 当前道路和转向

```text
N1|当前道路名|转向码\n
```

示例：

```text
N1|建国路|2\n
```

#### 下一道路

```text
N2|下一道路名\n
```

示例：

```text
N2|左安路\n
```

#### 距离信息

```text
D|距离下一路口|距离终点\n
```

示例：

```text
D|341|5200\n
```

推荐持续按这个顺序发送：

1. `N1`
2. `N2`
3. `D`

这样屏幕信息最完整。

### 常用转向码

常见图标码如下：

- `2`：左转
- `3`：右转
- `8`：左转掉头
- `9`：直行
- `11`：进入环岛
- `12`：驶出环岛
- `15`：到达目的地

如果你的数据源本身就使用高德导航图标码，可以直接复用。

### 方式二：micro-ROS

如果你想把这个设备接入 ROS 2 网络，可以使用 micro-ROS 模式。

#### 1. 配置本地 Wi-Fi 和 Agent 地址

在 `main/project_local_config.private.h` 中填写：

```c
#define PROJECT_WIFI_SSID "your_wifi_ssid"
#define PROJECT_WIFI_PASSWORD "your_wifi_password"
#define PROJECT_MICRO_ROS_AGENT_IP "192.168.1.100"
#define PROJECT_MICRO_ROS_AGENT_PORT "8888"
```

#### 2. 在 ROS 2 主机上启动 Agent

```bash
micro-ros-agent udp4 --port 8888 -v6
```

如果你已经安装了 ROS 2 包，也可以这样启动：

```bash
ros2 run micro_ros_agent micro_ros_agent udp4 --port 8888 -v6
```

#### 3. 重新烧录设备

```bash
idf.py build
idf.py flash monitor
```

#### 4. 发送测试消息

当前项目订阅的话题是：

- `/vehicle_detect`
- 类型：`std_msgs/msg/Bool`

示例：

```bash
ros2 topic pub /vehicle_detect std_msgs/msg/Bool "{data: true}" --rate 1
```

## 常见问题

### 屏幕不亮

- 检查供电
- 检查背光引脚
- 检查 `ST7789.h` 里的引脚配置是否和你的接线一致

### 画面方向不对

- 修改 `main/LVGL_Driver/LVGL_Driver.h` 中的镜像设置

### BLE 连上了但没有显示内容

- 确认写入的是 `0xFF01`
- 确认每条消息都以 `\n` 结尾
- 确认格式是 `N1`、`N2`、`D`

### micro-ROS 连不上

- 确认 Wi-Fi 已正确配置
- 确认设备和 Agent 在同一局域网
- 确认 Agent 正在监听 `8888/udp`

### 编译失败

- 确认已经正确导出 ESP-IDF 环境
- 确认当前终端没有被错误的 Python 环境干扰

## 你通常只需要改这几个地方

- `main/project_local_config.private.h`：本地 Wi-Fi 和 Agent 配置
- `main/LCD_Driver/ST7789.h`：屏幕引脚
- `main/LVGL_Driver/LVGL_Driver.h`：画面镜像方向


