# Chunked Streaming Runtime Design

## 1. 背景与问题

旧实现在 `chunked` 响应上默认“读完整包再返回”。这对常规 HTTP 够用，但对大模型长输出不友好：

- 首包晚，无法边到边消费；
- 长响应期间内存增长明显；
- 连接复用状态依赖“何时读完”，排障信息不足。

本设计的目标是：在不破坏现有默认行为的前提下，为客户端提供稳定、可排障的流式读取路径。

---

## 2. 设计目标与边界

### 2.1 目标

1. 通过开关启用 chunked 流式 body；
2. 仅在 `MESSAGE_COMPLETE` 时提交连接状态；
3. 异常信息可直接用于线上定位（断流、超时、解析卡住）；
4. 保持分层：协议层不依赖 PSR。

### 2.2 非目标

1. 不实现“未读完自动清尾再复用”；
2. 不在本次引入复杂策略枚举或后台回收协程。

选择原因：在线上场景里“未读完即异常分支”是小概率，直接断连比多策略更稳、更易维护。

---

## 3. 对外行为

### 3.1 开关

- 开启：`Client::setStreamingChunkedResponse(true)`
- 关闭：沿用旧行为（一次性拼完整 body）

### 3.2 流式语义

开启后，响应在 `headers complete` 后即可返回，`body` 为可读流，按读取动作推进网络解析。

### 3.3 连接策略（唯一行为）

- 如果上一个流式 body 未消费完就开始下一个请求：直接关闭连接并抛异常；
- 如果业务主动 `close()` 且 body 未完成：直接关闭连接。

---

## 4. 分层与核心对象

### 4.1 协议层（`Http/Protocol`）

- `ChunkedBodyState`
  - 保存连接内的流式解析状态：`buffer / parser / parsedOffset / bodyBuffer / currentChunkLength`
- `ChunkedBodyStream`
  - 协议层可读流；
  - 负责按需推进、缓存回放、完成回调。
- `ReceiverTrait`
  - 在 `recvMessageEntity()` 创建流式状态；
  - 用 `pumpChunkedBodyStateToLength()` 推进解析；
  - 在 `MESSAGE_COMPLETE` 执行 finalize。

### 4.2 PSR 层（`Psr7`）

- `ChunkedBodyPsrStream`
  - 组合适配 `ChunkedBodyStream`，实现 `StreamInterface`。
- `CreatorTrait`
  - 把协议层流包装为 PSR body；
  - 在流完成后（plus 对象）归一化响应头。

---

## 5. 两个 Buffer 的职责

- `state->buffer`：协议工作缓冲
  - 存原始网络字节；
  - parser 在此执行；
  - 已解析区可回收。

- `state->bodyBuffer`：业务数据缓冲
  - 仅存解析后的 body；
  - 支持重复读取；
  - 用于最终 `Content-Length` 计算。

这两个缓冲分离后，协议推进和业务读取互不干扰。

---

## 6. 关键时序

1. 收到 `chunked` 响应头且开关开启；
2. 创建 `ChunkedBodyState + ChunkedBodyStream` 并返回 response；
3. 业务调用 `read/getContents/toString`；
4. `pumpChunkedBodyStateToLength()` 驱动 parser 前进；
5. 到 `EVENT_MESSAGE_COMPLETE` 后 finalize：
   - reset parser；
   - 回收协议 buffer；
   - 清理 `activeChunkedBody`。

---

## 7. 异常模型与可观测性

### 7.1 连接状态异常

未消费完 body 再发请求时，抛 `ProtocolException`，消息包含：

- `buffered_bytes`
- `complete`

### 7.2 流式 IO 异常

`SocketException` 会附带上下文：

- `event`
- `parsed_offset`
- `buffer_length`
- `body_buffered`
- `current_chunk_length`

### 7.3 保护性失败

当协议缓冲已满且无可回收空间时，立即失败（`ParserException`），避免死循环和越界风险。

---

## 8. 头归一化规则

流完成后，仅对 plus 对象执行：

1. 从 `Transfer-Encoding` 移除 `chunked`
2. 写入最终 `Content-Length`

说明：非 plus 对象通常是不可变语义，无法在完成回调阶段原地回写实例。

---

## 9. 已落地测试

文件：`lib/swow-library/tests/Psr7/Client/ClientTest.php`

- `testStreamingChunkedBodyWillNormalizeHeadersAfterCompletion`
- `testStreamingChunkedBodyMustBeConsumedBeforeNextRequest`
- `testStreamingChunkedSocketExceptionContainsContext`

运行命令（仓库根）：

`php -d extension=swow vendor/bin/phpunit --no-coverage -c lib/swow-library/phpunit.xml.dist lib/swow-library/tests/Psr7/Client/ClientTest.php --filter "testStreamingChunkedBodyWillNormalizeHeadersAfterCompletion|testStreamingChunkedBodyMustBeConsumedBeforeNextRequest|testStreamingChunkedSocketExceptionContainsContext"`

---

## 10. 维护建议

后续改动优先保证三件事：

1. finalize 时机只能是 `MESSAGE_COMPLETE`；
2. 协议层与 PSR 层分层不反向依赖；
3. 异常信息字段保持稳定，便于线上检索与告警聚合。
