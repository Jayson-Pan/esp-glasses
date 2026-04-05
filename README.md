# ARnav ESP32

面向聋人使用场景的 ESP32-S3 智能眼镜字幕显示固件。设备通过 BLE 接收上位机发送的实时字幕消息，并在 `172x320` 的 ST7789 屏幕上显示定稿字幕、实时字幕和异常状态。

## 项目定位

当前版本专注于实时字幕显示：

- 接收语音识别或字幕服务生成的文本
- 通过 BLE 发送到眼镜端
- 以适合近眼显示的方式稳定呈现给用户

适用场景：

- 面对面交流字幕
- 课堂字幕提示
- 会议实时转写显示
- 手机或上位机驱动的辅助沟通终端

## 当前能力

- BLE GATT Server 广播与连接
- 接收分片字幕消息并在设备端重组
- 区分“实时字幕”和“定稿字幕”
- 支持会话状态、错误状态和清屏控制
- 基于 LVGL 的近眼显示界面
- 默认适配 ESP32-S3 + ST7789 `172x320` 屏幕

## 硬件要求

- ESP32-S3 开发板
- ST7789 SPI 屏幕，分辨率 `172x320`
- USB 数据线

默认屏幕引脚定义见 [`main/LCD_Driver/ST7789.h`](main/LCD_Driver/ST7789.h)：

| 信号 | 默认 GPIO |
|------|-----------|
| `SCLK` | `40` |
| `MOSI` | `45` |
| `DC` | `41` |
| `RST` | `39` |
| `CS` | `42` |
| `BK_LIGHT` | `46` |

如果你的硬件接线不同，请先修改这些宏再编译。

## 软件要求

- ESP-IDF `5.2` 或兼容的 `5.x`
- 已完成 ESP-IDF 工具链安装
- 首次构建时允许下载依赖组件

项目通过 [`main/idf_component.yml`](main/idf_component.yml) 引入：

- `lvgl/lvgl ~8.3.0`

## 快速开始

### 1. 导出 ESP-IDF 环境

```bash
source /path/to/esp-idf/export.sh
```

### 2. 编译并烧录

```bash
idf.py set-target esp32s3
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

退出串口监视器：

```bash
Ctrl+]
```

## 最短验证流程

烧录完成后，可以使用 `nRF Connect` 或任意 BLE 调试工具验证字幕链路：

1. 扫描并连接设备 `ESP_GATTS_DEMO`
2. 找到服务 `0x00FF`
3. 找到特征 `0xFF01`
4. 向特征写入一条完整 JSON 字幕消息
5. 屏幕应立即显示字幕内容

示例：

```json
{"type":"caption_update","sessionId":"demo-001","seq":1,"lineId":"line-001","speakerLabel":"Speaker","text":"你好，欢迎使用字幕眼镜。"}
```

## BLE 接口

当前固件提供两组可写特征，推荐优先使用主通道：

| 用途 | Service UUID | Characteristic UUID | 说明 |
|------|--------------|---------------------|------|
| 主通道 | `0x00FF` | `0xFF01` | 推荐使用 |
| 备用通道 | `0x00EE` | `0xEE01` | 兼容保留 |

默认广播设备名：

```text
ESP_GATTS_DEMO
```

消息发送建议：

- 一条字幕消息对应一个完整 JSON 对象
- 分片发送时，建议在消息末尾追加换行符 `\n`
- 长消息建议配合更大的 MTU 或 BLE Prepared Write

## 字幕协议

### 会话开始

```json
{"type":"session_state","sessionId":"demo-001","seq":0,"state":"started"}
```

说明：

- 初始化一个新的字幕会话
- 当 `sessionId` 改变时，设备会重置当前字幕状态

### 实时字幕

```json
{"type":"caption_update","sessionId":"demo-001","seq":1,"lineId":"line-001","speakerLabel":"Teacher","text":"今天我们开始第一章。"}
```

说明：

- 作为“实时字幕”显示
- 会覆盖当前实时行，但不会替换定稿行

### 定稿字幕

```json
{"type":"caption_commit","sessionId":"demo-001","seq":2,"lineId":"line-001","speakerLabel":"Teacher","text":"今天我们开始第一章。"}
```

说明：

- 作为“定稿字幕”显示
- 提交后会清空实时字幕行

### 清空实时字幕

```json
{"type":"caption_clear","sessionId":"demo-001","seq":3}
```

### 错误状态

```json
{"type":"session_state","sessionId":"demo-001","seq":4,"state":"error","errorMessage":"字幕流中断"}
```

说明：

- 错误信息会显示在实时字幕区域

### 字段兼容性

设备端兼容以下字段别名：

- 说话人字段：`speakerLabel`、`speaker`、`speakerName`、`speakerId`
- 文本字段：`text`、`content`、`message`、`caption`

协议约束：

- `seq` 必须递增，旧消息会被丢弃
- `caption_update` 用于实时字幕
- `caption_commit` 用于定稿字幕
- `caption_clear` 只清空实时字幕
- `session_state` 用于会话状态和异常提示

## 界面说明

当前 UI 位于 [`main/LVGL_UI/LVGL_Example.c`](main/LVGL_UI/LVGL_Example.c)。

界面布局：

- 左侧状态栏显示蓝牙图标和电池图标
- 顶部文本显示定稿字幕
- 中部文本显示实时字幕或错误提示

为了降低字库缺失导致的乱码风险，固件会把部分常见中文全角标点转换为半角标点后再显示。

## 你最可能会改的文件

| 文件 | 用途 |
|------|------|
| [`main/BLE_server/gatts_demo.c`](main/BLE_server/gatts_demo.c) | BLE 服务、消息接收、字幕协议解析 |
| [`main/LVGL_UI/LVGL_Example.c`](main/LVGL_UI/LVGL_Example.c) | 字幕界面布局和文本显示 |
| [`main/LCD_Driver/ST7789.h`](main/LCD_Driver/ST7789.h) | 屏幕引脚与分辨率配置 |
| [`main/LVGL_Driver/LVGL_Driver.h`](main/LVGL_Driver/LVGL_Driver.h) | 屏幕镜像与 LVGL 缓冲设置 |

## 显示方向调整

如果画面方向不对，可以修改 [`main/LVGL_Driver/LVGL_Driver.h`](main/LVGL_Driver/LVGL_Driver.h) 中的镜像开关：

```c
#define LVGL_MIRROR_HORIZONTAL 1
#define LVGL_MIRROR_VERTICAL 0
```

## 排查问题

### 屏幕不亮

优先检查：

- 供电是否稳定
- 背光引脚是否接对
- [`main/LCD_Driver/ST7789.h`](main/LCD_Driver/ST7789.h) 中的 GPIO 是否与实际接线一致

### BLE 已连接但没有显示字幕

优先检查：

- 是否写入了 `0xFF01` 或 `0xEE01`
- 是否发送了完整 JSON 对象
- 分片消息是否正确结束
- `sessionId` 与 `seq` 是否符合协议要求

### 长消息显示不完整

可能原因：

- BLE MTU 太小
- 客户端未正确处理分片发送
- 单条字幕过长

建议：

- 优先协商更大的 MTU
- 一条消息只发送一个 JSON 对象
- 分片消息末尾追加 `\n`

### 编译失败

请确认：

- 当前终端已经执行过 `export.sh`
- 使用的是 ESP-IDF 5.x 环境
- 依赖组件可以正常下载

必要时可以清理后重试：

```bash
idf.py fullclean
idf.py build
```
