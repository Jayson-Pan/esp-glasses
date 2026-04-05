# BLE 字幕协议草案

> 状态：协议草案  
> 目标：用于手机 App 与 ESP32 之间的实时字幕传输  
> 当前阶段：仅写入 Markdown，不做实现

## 1. 协议目标

这个协议不是通用 BLE 文本协议，而是专门服务于“听悟实时字幕上屏”场景。

它要解决的核心问题是：

1. 字幕来自实时 ASR，`partial` 会频繁变化
2. BLE GATT 单包小，必须分片
3. 中文 UTF-8 不能直接按字符截断
4. ESP32 屏幕不应该做“逐字 append”，而应该做“当前字幕覆盖刷新”
5. `partial` 和 `final` 的优先级不同
6. 网络波动、重复包、乱序包都必须能容忍

## 2. 协议设计原则

## 2.1 逻辑消息和传输帧分离

协议分两层：

- **逻辑消息**
  表示“要显示什么”
- **传输帧**
  表示“这条逻辑消息如何通过 BLE 分包发出去”

这样做的原因是：

- Flutter 侧可以按字幕语义产生命令
- ESP32 侧可以先重组，再显示
- BLE MTU 变化时，不用改字幕语义层

## 2.2 以“字幕状态更新”为核心，不以“字符流追加”为核心

协议里的最小显示单位不是“单个字符”，而是：

- 一条当前字幕快照

因此 ESP32 端的更新语义应该是：

- `replace current line`
- `commit current line`

而不是：

- `append one more char`

## 2.3 partial 和 final 处理不同

- `partial`
  高频、可覆盖、允许丢弃旧版本
- `final`
  必须可靠送达、优先级更高、用于固化显示

## 2.4 先重组，再 UTF-8 解码

无论手机还是 ESP32：

- 不允许对单个 BLE 分片直接做 UTF-8 解码
- 必须先按 `messageId` 重组完整 payload
- 完整重组后再做 UTF-8 解码

## 3. 总体结构

```text
Tingwu 实时事件
  -> Flutter 生成逻辑字幕消息
  -> 逻辑消息序列化为 UTF-8 JSON
  -> JSON 字节流按 BLE MTU 分片
  -> 分片发送到 ESP32
  -> ESP32 按 messageId 重组
  -> CRC 校验
  -> UTF-8 解码
  -> LVGL 更新显示
```

## 4. 逻辑消息定义

## 4.1 消息类型

逻辑消息只定义 4 类：

1. `caption_update`
2. `caption_commit`
3. `caption_clear`
4. `session_state`

## 4.2 `caption_update`

用于实时 partial 刷新。

语义：

- 覆盖当前字幕显示区
- 不代表这句话已经定稿

推荐字段：

```json
{
  "type": "caption_update",
  "sessionId": "sess_1711857000",
  "messageId": 1024,
  "lineId": "s_42",
  "seq": 18,
  "speakerLabel": "A",
  "text": "明天下午三点",
  "isFinal": false,
  "timestamp": 1711857000123
}
```

字段说明：

- `type`
  固定为 `caption_update`
- `sessionId`
  当前监听会话 ID，用于避免跨会话污染
- `messageId`
  这条逻辑消息的唯一 ID
- `lineId`
  当前句子的稳定标识，优先用 `sentenceId`
- `seq`
  当前会话内递增序号，便于 ESP32 丢弃旧消息
- `speakerLabel`
  可选，例 `A`、`B`
- `text`
  当前 partial 文本
- `isFinal`
  此类型下固定为 `false`
- `timestamp`
  发送时间，便于调试

## 4.3 `caption_commit`

用于当前句子定稿。

语义：

- 当前字幕已经稳定
- ESP32 可以把它固化为上一条历史或当前稳定行

推荐字段：

```json
{
  "type": "caption_commit",
  "sessionId": "sess_1711857000",
  "messageId": 1025,
  "lineId": "s_42",
  "seq": 19,
  "speakerLabel": "A",
  "text": "明天下午三点开会",
  "isFinal": true,
  "timestamp": 1711857001456
}
```

与 `caption_update` 的主要差异：

- `type = caption_commit`
- `isFinal = true`

## 4.4 `caption_clear`

用于清空当前实时显示区。

适用场景：

- 长时间静默
- 切换页面
- 用户手动停止
- BLE 重连后重置状态

示例：

```json
{
  "type": "caption_clear",
  "sessionId": "sess_1711857000",
  "messageId": 1026,
  "seq": 20,
  "timestamp": 1711857002000
}
```

## 4.5 `session_state`

用于会话生命周期通知，不携带字幕文本。

建议状态：

- `started`
- `stopping`
- `stopped`
- `error`

示例：

```json
{
  "type": "session_state",
  "sessionId": "sess_1711857000",
  "messageId": 1001,
  "seq": 1,
  "state": "started",
  "timestamp": 1711857000000
}
```

错误场景：

```json
{
  "type": "session_state",
  "sessionId": "sess_1711857000",
  "messageId": 1030,
  "seq": 24,
  "state": "error",
  "errorCode": "ble_disconnected",
  "errorMessage": "BLE disconnected",
  "timestamp": 1711857005000
}
```

## 5. 传输帧定义

## 5.1 为什么需要帧层

BLE GATT 单次有效负载通常远小于一条完整字幕消息，所以逻辑消息必须切成多帧。

推荐使用固定头 + 可变 payload 的二进制帧。

## 5.2 帧结构

建议结构如下：

| 字段 | 长度 | 说明 |
|------|------|------|
| `version` | 1 byte | 协议版本，当前固定 `0x01` |
| `frameType` | 1 byte | 帧类型 |
| `messageId` | 2 bytes | 逻辑消息 ID，uint16 |
| `sequence` | 1 byte | 当前分片序号，从 0 开始 |
| `total` | 1 byte | 分片总数 |
| `flags` | 1 byte | 位标记 |
| `payloadLength` | 2 bytes | 本帧 payload 字节长度 |
| `payload` | N bytes | 本帧数据 |
| `crc16` | 2 bytes | 帧校验 |

## 5.3 `frameType` 取值

建议取值：

| 值 | 含义 |
|------|------|
| `0x01` | 数据帧 |
| `0x02` | ACK |
| `0x03` | NACK |
| `0x04` | 心跳 |
| `0x05` | 状态查询 |

## 5.4 `flags` 位定义

建议：

- bit0：是否 `final`
- bit1：是否高优先级
- bit2：是否需要 ACK
- bit3：是否允许覆盖旧 `partial`
- 其余保留

推荐规则：

- `caption_update`
  - 需要 ACK：否
  - 高优先级：否
  - 可覆盖旧 partial：是
- `caption_commit`
  - 需要 ACK：是
  - 高优先级：是
  - 可覆盖旧 partial：否
- `session_state`
  - `started/stopping/stopped/error` 建议需要 ACK

## 6. 分片规则

## 6.1 MTU 策略

BLE 连接建立后：

1. 手机尽量协商更大的 MTU
2. 根据实际 MTU 动态计算单帧最大 payload
3. 永远按**字节数**切分，不按字符数切分

## 6.2 分片方式

流程：

1. 逻辑消息先转成 UTF-8 JSON 字节流
2. 再按 `maxPayloadSize` 切成多个帧
3. 每帧写入：
   - 相同的 `messageId`
   - 自己的 `sequence`
   - 相同的 `total`

例如一条 JSON 长度为 170 bytes，单帧 payload 上限 40 bytes：

- 分成 5 帧
- `sequence = 0,1,2,3,4`
- `total = 5`

## 6.3 重组规则

ESP32 收到数据帧后：

1. 按 `messageId` 找缓冲区
2. 按 `sequence` 填入对应槽位
3. 如果已收齐 `0..total-1`
4. 校验通过后拼接成完整 payload
5. 再 UTF-8 解码并反序列化 JSON

## 7. ACK / NACK 规则

## 7.1 为什么不是所有消息都要 ACK

如果所有 `partial` 都要求 ACK，会带来：

- 频繁往返
- BLE 拥塞
- ESP32 刷新滞后

所以建议分级：

- `partial update`
  默认不要求 ACK
- `final commit`
  必须 ACK
- `session state`
  建议 ACK

## 7.2 ACK 消息结构

ACK 本身只需要最小字段：

```json
{
  "type": "ack",
  "messageId": 1025,
  "sessionId": "sess_1711857000",
  "timestamp": 1711857001600
}
```

如果继续走二进制帧，也可以只回：

- `frameType = 0x02`
- `messageId`
- `status = ok`

## 7.3 NACK 消息结构

用于 CRC 错误或关键分片缺失：

```json
{
  "type": "nack",
  "messageId": 1025,
  "sessionId": "sess_1711857000",
  "reason": "crc_failed"
}
```

## 7.4 重传策略

建议：

- `caption_update`
  - 不主动重传旧消息
  - 直接发送更新后的新版本即可
- `caption_commit`
  - 超时未 ACK 时重传
  - 最多重传 `2~3` 次
- `session_state`
  - 超时未 ACK 时可重传

## 8. 去重与覆盖规则

## 8.1 手机端规则

手机端每次发送前应判断：

1. 文本是否变化
2. 是否到达节流窗口
3. 当前 `lineId` 是否还是同一句

如果 `partial` 文本没变化，就不发。

## 8.2 ESP32 端规则

ESP32 应维护：

- `lastSessionId`
- `lastSeq`
- `currentLineId`

处理规则：

1. 如果 `sessionId` 变了，重置当前字幕状态
2. 如果 `seq` 小于当前已处理序号，丢弃
3. 如果 `lineId` 相同且类型是 `caption_update`，覆盖当前行
4. 如果 `lineId` 相同且类型是 `caption_commit`，固化当前行
5. 如果 `lineId` 不同，说明进入新句子

## 9. ESP32 显示语义

## 9.1 单行模式

适用于极小屏幕。

显示规则：

- `caption_update`
  覆盖当前整行
- `caption_commit`
  覆盖当前整行，并保持到下一句到来

优点：

- 实现最简单
- 屏幕稳定

缺点：

- 没有历史

## 9.2 双行模式

适用于能同时显示两行的屏幕。

显示规则：

- 上一行：最近一条 `commit`
- 下一行：当前 `update` 或当前 `commit`

例如：

```text
A：明天下午三点开会
B：地点改到二楼会议室
```

## 9.3 speaker 规则

V1 建议：

- 如果消息里有 `speakerLabel`，就显示
- 没有就只显示文本

不要在 ESP32 端自行推测 speaker。

## 10. Flutter 侧发送策略

## 10.1 发送触发条件

推荐只在下面几种情况下发 BLE：

1. 新的 `caption_update`
2. 新的 `caption_commit`
3. `session_state` 变化
4. 长时间静默需要 `caption_clear`

## 10.2 节流建议

为避免高频刷新导致 BLE 拥塞，建议：

- `caption_update`
  每 `100~200ms` 最多发送一次
- `caption_commit`
  立即发送
- `session_state`
  立即发送

## 10.3 优先级建议

发送优先级从高到低：

1. `session_state:error`
2. `caption_commit`
3. `session_state:stopped`
4. `caption_update`
5. 心跳

## 11. 错误处理

## 11.1 BLE 断开

如果 BLE 断开：

- 手机端继续维持 Tingwu 会话可选
- 但建议立刻标记“眼镜端不可用”
- 重连后先发：
  - `session_state`
  - 最新一条 `caption_commit`
  - 当前最新 `caption_update`

## 11.2 ESP32 重启

ESP32 重启后：

- 手机收到心跳异常或连接重建
- 重新发送当前显示状态

## 11.3 UTF-8 解码失败

如果 ESP32 重组后 UTF-8 解码失败：

- 丢弃该条消息
- 回 `NACK`
- 手机仅对 `final` 或关键状态消息考虑重传

## 12. 示例消息

## 12.1 开始会话

```json
{
  "type": "session_state",
  "sessionId": "sess_001",
  "messageId": 1,
  "seq": 1,
  "state": "started",
  "timestamp": 1711857000000
}
```

## 12.2 partial 更新

```json
{
  "type": "caption_update",
  "sessionId": "sess_001",
  "messageId": 2,
  "lineId": "line_001",
  "seq": 2,
  "speakerLabel": "A",
  "text": "明天下午三点",
  "isFinal": false,
  "timestamp": 1711857000100
}
```

## 12.3 final 定稿

```json
{
  "type": "caption_commit",
  "sessionId": "sess_001",
  "messageId": 3,
  "lineId": "line_001",
  "seq": 3,
  "speakerLabel": "A",
  "text": "明天下午三点开会",
  "isFinal": true,
  "timestamp": 1711857001200
}
```

## 12.4 停止会话

```json
{
  "type": "session_state",
  "sessionId": "sess_001",
  "messageId": 4,
  "seq": 4,
  "state": "stopped",
  "timestamp": 1711857005000
}
```

## 13. 推荐版本边界

## V1 必做

- `session_state`
- `caption_update`
- `caption_commit`
- UTF-8 安全重组
- `seq` 去旧
- `final` ACK

## V1 可不做

- 复杂历史缓存
- 全量重传队列
- speaker 强依赖
- 压缩编码

## V2 再做

- NACK 精细化
- 会话恢复同步
- 2 行历史缓存
- 更复杂的屏幕分页策略

## 14. 最终结论

这个 BLE 字幕协议草案的核心思想是：

- 不传“字符流”
- 传“字幕状态消息”
- partial 覆盖当前行
- final 固化当前句
- 先重组，再解码
- 关键消息可靠送达，非关键消息允许被新版本覆盖

如果实现时严格遵守这几个原则，它会比“直接发字符串到 ESP32”稳定很多，也更适合你要的聋人实时字幕屏场景。
