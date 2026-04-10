# H2 Server Runtime Design

## 1. 背景与目标

当前 Swow 的 HTTP 收发主链路仍基于 HTTP/1.x parser：

- `ext/src/swow_http.c` 绑定 `cat_http_parser_*`，能力模型是文本 HTTP；
- `lib/swow-library/src/Http/Protocol/ReceiverTrait.php` 负责 HTTP/1.x request/response 解析；
- `lib/swow-library/src/Psr7/Server/ServerConnection.php` 直接继承 `Socket`，对外暴露的是 HTTP/1.x / WebSocket 语义。

本设计的目标是：在 **不修改现有 C 层 HTTP/1.x parser**、**不破坏现有 HTTP/1.x API** 的前提下，为 `feature_h2` 提供一条独立的 PHP 层 HTTP/2 服务端实现路径。

首版边界：

1. 只做服务端，不做客户端；
2. 优先支持 `TLS ALPN h2`，`h2c` 作为第二阶段；
3. 先打通协议底座和最小请求/响应闭环；
4. 高级能力先保留底座，不急于暴露业务 API。

---

## 2. 远端 `packages/h2` 功能清单

上游参考：`php-standard-library/php-standard-library` 的 `packages/h2`。

### 2.1 已确认的完整能力

#### 连接与角色模型

- `ClientConnection` / `ServerConnection`
  - 分别提供 client/server 端连接对象；
  - server 端显式校验 client preface。
- `ConnectionInterface`
  - 统一抽象初始化、读事件、发 headers/data、流控等待、`RST_STREAM`、`PING`、`GOAWAY`。
- `Internal/StateMachine.php`
  - 维护连接关闭状态、stream id 分配、last peer stream id、active stream 计数。

#### 帧层

- `Frame/*`
  - 已实现 `DATA`、`HEADERS`、`CONTINUATION`、`SETTINGS`、`WINDOW_UPDATE`、`RST_STREAM`、`PING`、`GOAWAY`；
  - 还实现了 `PUSH_PROMISE`、`PRIORITY`、`PRIORITY_UPDATE`、`ALTSVC`、`ORIGIN`。
- `Frame/decode.php` / `Frame/encode.php`
  - 提供二进制帧编码与解码。

#### 头部与 HPACK

- `Internal/HeaderBlockAssembler.php`
  - 重组跨 `CONTINUATION` 的 header block。
- `Internal/HeaderValidator.php`
  - 校验 pseudo-header 顺序、重复、大小写、`CONNECT` / extended CONNECT 约束。
- `packages/hpack`
  - 上游单独提供 HPACK `Encoder` / `Decoder`。

#### 状态机与流生命周期

- `Internal/StreamTable.php` / `Internal/StreamEntry.php`
  - 管理 stream 表、活动数、窗口初值、并发上限。
- `StreamState.php`
  - 覆盖 `idle/open/half-closed/reserved/closed`。
- `StateMachine::receive*()`
  - 负责帧分派、状态迁移、用户事件产出。

#### 流控与可靠性

- `Internal/FlowController.php`
  - 分别管理 connection-level 和 stream-level send/receive window。
- `ConnectionTrait::waitForSendWindow()`
  - 支持等待窗口可用后再发。
- `StateMachine::receiveDataRaw()`
  - 消耗 receive window，并自动补 `WINDOW_UPDATE`。
- `RateLimiter.php`
  - 针对 `SETTINGS` / `PING` / `RST_STREAM` / `PRIORITY` / empty DATA 做速率限制。

#### 事件模型

- `Event/*`
  - 已建模 `HeadersReceived`、`DataReceived`、`SettingsReceived`、`PingReceived`、`GoAwayReceived`、
    `StreamReset`、`StreamClosed`、`WindowUpdated`；
  - 还建模了 `PushPromiseReceived`、`PriorityUpdateReceived`、`AltSvcReceived`、`OriginReceived`。

#### 性能与优化

- `ConnectionTrait`
  - 支持 write buffer 聚合写。
- `Internal/BDPEstimator.php`
  - 基于 `PING ACK` 和吞吐量估算接收窗口。

### 2.2 需要保守看待的点

1. 未在上游包目录中看到随包分发的 `tests/`；
2. 因此不能把“存在类与方法”直接视为“已完成充分回归”；
3. Swow 引入时必须重新建立自己的最小回归集。

---

## 3. Swow 差距分析与接入方案

### 3.1 当前 Swow 现状

#### 已具备能力

- Socket/TLS 基础能力已具备；
- `enableCrypto()` 已暴露 `alpn_protocols` 选项；
- `ext/tests/swow_socket/ssl_alpn.phpt` 已证明 Swow TLS 通道可协商到 `h2`；
- `UpgradeType::fromString()` 已能识别 `h2c`。

#### 尚不具备能力

- 没有 HTTP/2 二进制帧 parser / encoder；
- 没有 HPACK 编解码；
- 没有 HTTP/2 stream 状态机与流控；
- `Psr7\Server\ServerConnection` 只适配 HTTP/1.x 单请求收发，不适合直接承载 H2 多路复用。

### 3.2 接入原则

1. **不改 C 层 HTTP/1.x parser**
   - H2 是二进制 framing，不应强塞进现有 `llhttp` 语义。
2. **不破坏现有 HTTP/1.x API**
   - `Client` / `ServerConnection` 现有签名不动。
3. **PHP 层独立实现 H2 server runtime**
   - 直接基于 `Swow\Socket` 收发 H2 二进制帧。
4. **协议层与 Psr7 适配层分层**
   - 协议底座只做连接、流、帧、事件；
   - Psr7 层只负责把 stream 映射成 request/response。

### 3.3 首版建议分层

#### 协议层

新增独立 H2 运行时模块，职责仅包括：

- 读取 client preface；
- 发送/接收 `SETTINGS`；
- 解析 `HEADERS/CONTINUATION/DATA/WINDOW_UPDATE/RST_STREAM/PING/GOAWAY`；
- 维护 stream state 与基础流控；
- 向上层抛出 stream 级事件。

#### Psr7 适配层

新增独立 H2 server connection 适配对象，职责包括：

- 从 H2 headers 构造 `ServerRequestInterface`；
- 把响应拆成 `:status + headers + data`；
- 管理单个 stream 的响应发送与关闭；
- 保持与现有 HTTP/1.x `ServerConnection` 并行，而非替换。

#### 连接入口

- `TLS ALPN h2`
  - 作为首版默认入口；
  - 在握手后分流到 H2 连接实现。
- `h2c`
  - 第二阶段补；
  - 不能影响当前 HTTP/1.x upgrade 路径稳定性。

### 3.4 首版对外暴露能力

首版建议只承诺：

1. 单连接初始化与 preface/SETTINGS 协商；
2. 基础 request HEADERS 读取；
3. 基础 response HEADERS + DATA 发送；
4. 基础 `WINDOW_UPDATE` / `PING` / `RST_STREAM` / `GOAWAY`；
5. 多 stream 并发不串流。

首版不对业务 API 承诺：

- server push；
- `ORIGIN`；
- `ALTSVC`；
- `PRIORITY` / `PRIORITY_UPDATE`；
- extended CONNECT。

这些能力可以在协议底座中保留扩展位，但不进入首版公开语义。

---

## 4. 待确认问题

### 4.1 依赖策略

问题：HPACK 与状态机是直接移植上游，还是按 Swow 风格重写最小子集？

已知事实：

- 上游 H2 依赖 `packages/hpack`；
- 上游连接实现还依赖 `Psl\Async` / `Revolt\EventLoop` 风格的取消与挂起。

影响：

- 直接移植快，但会引入额外风格与依赖面；
- 重写更贴合 Swow，但前期成本更高。

默认建议：

- **状态机与帧模型可参考上游实现，异步等待语义改写为 Swow 风格；**
- **HPACK 可优先采用“内聚移植”而非在公共 API 中散落外部依赖。**

### 4.2 `h2c` 交付时机

问题：`h2c` 是否必须和 `ALPN h2` 同批交付？

已知事实：

- 本地已有 `UpgradeType::UPGRADE_TYPE_H2C`；
- 但没有现成 H2 明文升级实现。

默认建议：

- 首版先交付 `ALPN h2`；
- `h2c` 放到第二阶段，避免把首版入口复杂度拉高。

### 4.3 入口形态

问题：H2 是新增独立 `H2ServerConnection`，还是扩展现有 server factory 分流？

默认建议：

- 两者都要做，但顺序上是：
  1. 先新增独立 `H2ServerConnection`；
  2. 再通过 factory/accept 路径分流接入。

原因：

- 先做独立对象更容易测试；
- 避免早期把协议分流逻辑和协议本体耦在一起。

### 4.4 高级能力暴露边界

问题：`server push / origin / alt-svc` 是否只保留底座，不暴露对外 API？

默认建议：

- 是。首版先不暴露业务接口。

原因：

- 这些能力不是最小可用服务端闭环所必需；
- 过早暴露只会扩大测试与兼容面。

---

## 5. 优化建议

### 5.1 先做最小可用协议子集

不要一开始就把上游全部帧类型和事件全部接到 Swow 对外 API。

首批必须实现：

- preface；
- `SETTINGS`；
- `HEADERS/CONTINUATION`；
- `DATA`；
- `WINDOW_UPDATE`；
- `RST_STREAM`；
- `PING`；
- `GOAWAY`；
- 基础 header 校验。

### 5.2 成熟终版判定标准

“完全成熟版 H2 版本”在本仓库中的含义，不是“代码大部分已经存在”，而是同时满足下面 4 类条件：

#### A. 协议能力完整

必须满足：

1. TLS 连接可基于 ALPN 自动分流到 `h2` / `http/1.1`；
2. 明文连接可通过 `h2c` 正常升级；
3. request / response 均支持：
   - `HEADERS/CONTINUATION`
   - `DATA`
   - `SETTINGS`
   - `WINDOW_UPDATE`
   - `PING`
   - `RST_STREAM`
   - `GOAWAY`
   - request trailer
   - response trailer
4. HPACK 至少对真实客户端常见场景可用：
   - Huffman 解码可用；
   - dynamic table 可跨多个 header block 正常演进；
   - table size update 可正常处理；
5. 对服务端明确不支持的 frame / 行为，必须显式报协议错误，而不是静默忽略或退化。

#### B. 调度与流控成熟

必须满足：

1. 多个 stream 并发交错时：
   - 不串流；
   - 不丢帧；
   - 不死循环；
   - 不因某个 stream 的窗口阻塞而长期饿死其他 stream；
2. 请求读取、body 读取、trailer 读取、响应发送、窗口等待，必须共享同一套一致的调度语义；
3. 发送窗口耗尽后，能等待 `WINDOW_UPDATE` 恢复并继续发送；
4. `GOAWAY`、`RST_STREAM`、`WINDOW_UPDATE` 在等待窗口和多 stream 交错场景下行为稳定；
5. pending / defer 队列必须具备明确的控制帧优先与 stream 间公平推进规则。

#### C. 状态机与错误语义完整

必须满足：

1. 关键 stream 状态至少覆盖：
   - `idle`
   - `open`
   - `half_closed_remote`
   - `half_closed_local`
   - `closed`
2. frame/state 组合必须具备明确语义：
   - 合法时继续推进；
   - 非法时明确区分 connection error / stream error；
3. 旧 stream、已关闭 stream、`GOAWAY` 后新 stream、`RST_STREAM` 后重入，行为必须一致；
4. 不允许依赖 silent fallback 或模糊行为来“让它继续跑”。

#### D. 真实运行验证通过

必须满足：

1. 新扩展编译后，`Swow\Socket::getNegotiatedAlpnProtocol()` 在真实 TLS 连接上返回预期值；
2. 同一个 server 下，TLS+h2、TLS+HTTP/1.1、`h2c` 三条入口都能正常工作；
3. `EventDriver`、`Server::handleH2Connection()`、直连 H2 路径，行为一致；
4. 真实客户端（至少 curl / 业务客户端）下，大 header、大 body、小窗口、多 stream、trailer 组合场景稳定。

只有 A/B/C/D 四类都满足，才可把该实现称为“完全成熟版 H2 版本”。

### 5.3 当前实现与成熟终版的关系

当前实现已经满足：

- H2 服务端主体链路已经存在；
- TLS 自动 H2 入口、`h2c`、request/response、trailer、流控、`GOAWAY/RST_STREAM`、HPACK 核心路径均已接入；
- 自动化回归已能覆盖大部分协议与 server 入口路径；
- 当前 H2/server 回归基线为：
  - `Tests: 174`
  - `Assertions: 513`
  - `Skipped: 2`（当前环境 bind/listen 限制）

当前实现仍不应在文档里直接宣称为“完全成熟终版”，原因只有两类：

1. **代码成熟度边界**
   - 多 stream 调度已明显成熟，但还不是一个完全独立、可证明公平性的完整 scheduler 框架；
   - RFC 状态矩阵已覆盖很多关键组合，但还不是“所有 frame/state 组合全覆盖”。
2. **真实运行验证边界**
   - 新扩展 getter、TLS/ALPN 自动分流、真实 `h2c`、真实客户端长期交互，还需要本地实跑验证。

因此当前更准确的定位是：

- **高完整度 H2 服务端候选版本**
- **可进入本地真实流量验证**
- **接近可上线候选版**

而不是文档意义上的“完全成熟终版”。

### 5.4 本地验证重点清单

在进入生产前，至少需要完成下面的本地验证：

#### TLS / ALPN

- `getNegotiatedAlpnProtocol()`：
  - TLS+h2 返回 `h2`
  - TLS+HTTP/1.1 返回 `http/1.1`
  - 非 TLS / 未协商返回 `null`
- 同一 server 下 H2 和 HTTP/1.1 混跑不互相污染

#### h2c

- 正常升级
- 缺失 `HTTP2-Settings`
- 非法 `HTTP2-Settings`
- 升级请求带 body
- 升级后继续多个 stream

#### trailer

- request trailer 可读
- response trailer 可发
- trailer 含 pseudo-header 明确失败

#### 多 stream + 小窗口

- 大响应发送到一半窗口耗尽
- 期间到来 stream `3/5` 新请求
- 连续 `WINDOW_UPDATE` 恢复
- 期间混入 `RST_STREAM`
- 期间混入 `GOAWAY`
- 期间混入 `HEADERS + CONTINUATION`

#### 状态机边界

- even stream / `stream 0` 的 request 类帧
- 已关闭 stream 再收 `DATA/HEADERS/CONTINUATION/WINDOW_UPDATE`
- `GOAWAY(lastStreamId)` 后旧 stream 收尾、新 stream 拒绝
- `RST_STREAM` 后同 stream 再次收帧

### 5.2 把协议状态与 Psr7 对象解耦

不要让 `ServerRequest` / `Response` 直接持有协议底层可变状态。

推荐做法：

- 协议层维护 stream table；
- Psr7 层只拿只读请求视图和受控响应写口。

### 5.3 明确首版错误暴露

不要做 silent fallback，不要自动降级成 HTTP/1.1。

推荐做法：

- preface 错误：直接失败并关闭连接；
- 帧级协议错误：显式 `GOAWAY`；
- stream 级错误：显式 `RST_STREAM`；
- 流控错误：直接暴露异常并带 stream id / window 上下文。

### 5.4 先建立 Swow 自己的回归集

建议至少覆盖：

1. `TLS ALPN h2` 协商成功；
2. preface + 初始 `SETTINGS`；
3. 单 stream request/response；
4. 双 stream 并发；
5. 大 body 分片与窗口回补；
6. 非法 pseudo-header；
7. `CONTINUATION` 被打断；
8. `WINDOW_UPDATE` overflow；
9. `GOAWAY` 后拒绝新 stream。

---

## 6. 首版实施建议

建议按以下顺序推进：

1. 先新增 H2 分析文档；
2. 再做协议底座最小骨架；
3. 再做独立 H2 server connection；
4. 最后再把 server accept 路径接入协议分流；
5. `h2c` 与高级能力后置。

这样可以保证每一步都能独立验证，并且不破坏现有 HTTP/1.x 路径。

---

## 7. 证据索引

### 本地仓库

- `ext/src/swow_http.c`
- `ext/src/swow_socket.c`
- `ext/tests/swow_socket/ssl_alpn.phpt`
- `lib/swow-library/src/Http/Protocol/ReceiverTrait.php`
- `lib/swow-library/src/Psr7/Server/Server.php`
- `lib/swow-library/src/Psr7/Server/ServerConnection.php`
- `lib/swow-library/src/Psr7/Message/UpgradeType.php`

### 上游参考仓库

- `packages/h2/src/Psl/H2/ConnectionInterface.php`
- `packages/h2/src/Psl/H2/ClientConnection.php`
- `packages/h2/src/Psl/H2/ServerConnection.php`
- `packages/h2/src/Psl/H2/Internal/ConnectionTrait.php`
- `packages/h2/src/Psl/H2/Internal/StateMachine.php`
- `packages/h2/src/Psl/H2/Internal/FlowController.php`
- `packages/h2/src/Psl/H2/Internal/HeaderValidator.php`
- `packages/h2/src/Psl/H2/RateLimiter.php`
- `packages/hpack/src/Psl/HPACK/Encoder.php`
- `packages/hpack/src/Psl/HPACK/Decoder.php`
