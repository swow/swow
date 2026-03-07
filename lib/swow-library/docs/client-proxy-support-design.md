# MagicClient 与代理能力下沉设计

## 1. 核心结论

本次重构采用“两层模型”：

1. 代理能力下沉到底层传输层（非 `Psr7`）；
2. `Psr7` 层新增傻瓜化门面客户端：`MagicClient`。

`Psr7\Client\Client` 不再承载代理握手细节，只做 PSR 请求发送适配。

---

## 2. 为什么要重构

当前 `Psr7\Client\Client` 直接继承 `Socket`，在单类内同时承担：

- 连接行为；
- 请求打包；
- 协议读取；
- 异常转换。

问题是代理需求（SOCKS5 / HTTP CONNECT）本质属于“传输链路能力”，放在 PSR 封装层会导致：

- 职责混杂；
- 扩展点受限；
- 后续高层 API（易用客户端）难做。

---

## 3. 新分层

## 3.1 底层传输层（新增）

建议新增模块：`src/Http/Client/` 与 `src/Http/Proxy/`

- `Http/Proxy/ProxyType`
- `Http/Proxy/ProxyConfig`
- `Http/Proxy/ProxyHandshake`
- `Http/Client/TransportClient`（组合 `Socket`，不暴露 PSR）

职责：

- 直连 / HTTP 代理 / SOCKS5 统一连接；
- HTTP CONNECT 隧道；
- SOCKS5 协商与认证；
- TLS 自动时机控制；
- 传输级异常上下文输出。

## 3.2 Psr7 基础层（保留）

`src/Psr7/Client/Client.php` 改为组合 `TransportClient`：

- 接收 `RequestInterface`；
- 构造 headers/body；
- 调用底层发送与接收；
- 把 `ResponseEntity` 转回 `ResponseInterface`。

## 3.3 Psr7 门面层（新增）

新增：`src/Psr7/Client/MagicClient.php`

目标：给业务方“开箱即用”的 API，不暴露底层 socket 细节。

同时新增：`src/Psr7/Client/ClientPlusInterface.php`

目标：在不破坏 PSR-18 的前提下，为客户端增加 SSE 扩展能力。

---

## 4. MagicClient 对外 API

建议首版公开以下接口：

- `sendRequest(RequestInterface $request): ResponseInterface`（PSR-18 标准入口）
- `request(string $method, string $url, array $options = []): ResponseInterface`
- `get(string $url, array $options = []): ResponseInterface`
- `post(string $url, array $options = []): ResponseInterface`
- `stream(string $url, array $options = []): \Generator`（SSE 场景，内部走 `Psr7::readEventStream()`，支持 `POST`）
- `sendEventStreamRequest(RequestInterface $request, ?int $timeout = null, int $readSize = 8192): \Generator`（`ClientPlusInterface` 扩展）
- `setProxy(?array $proxy): static`（为 PSR-18 `sendRequest()` 提供默认代理配置）
- `setTlsOptions(array $tlsOptions): static`（为 HTTPS 连接提供默认 TLS 配置）

`$options` 统一字段：

- `headers`（array）
- `query`（array）
- `json`（array）
- `body`（string|resource|StreamInterface）
- `timeout`（int）
- `proxy`（`type/host/port/username/password/remoteDns`）
- `streaming_chunked`（bool）
- `tls`（如 `peer_name` 等扩展项）
- `method`（仅 `stream()` 使用，默认 `GET`，可设 `POST/PUT/PATCH/...`）

约束：

- `json` 与 `body` 互斥；
- 未传 `timeout` 使用统一默认值；
- 统一自动补 `Host` / `Connection` / `Content-Length`。

---

## 5. MagicClient 的 PSR 合规边界

`MagicClient` 必须实现 `Psr\Http\Client\ClientInterface`，并保证 `sendRequest()` 的行为符合 PSR-18：

1. 接口签名保持标准：`sendRequest(RequestInterface): ResponseInterface`；
2. `4xx/5xx` 返回正常 `ResponseInterface`，不当作异常；
3. 仅网络故障、协议错误、超时等场景抛出 `ClientExceptionInterface`；
4. 便捷接口（`request/get/post/stream`）是增强层，不影响标准入口。

这样 `MagicClient` 既可“傻瓜化调用”，也可作为标准 PSR-18 客户端接入中间件生态。

---

## 6. 代理行为标准化

## 6.1 HTTP Proxy + HTTP

- 传输连接：`proxyHost:proxyPort`
- 请求行：absolute-form（`GET http://target/path HTTP/1.1`）
- `Host`：目标站点，不是代理站点
- 可选 `Proxy-Authorization`

## 6.2 HTTP Proxy + HTTPS

1. 先连代理；
2. 发 `CONNECT targetHost:targetPort`；
3. 收到 200 后在同一 socket 上启用 TLS；
4. 后续请求行恢复 origin-form。

## 6.3 SOCKS5 + HTTP/HTTPS

1. 先连 SOCKS5；
2. 方法协商（NO_AUTH / USERPASS）；
3. 认证（可选）；
4. 发 SOCKS5 CONNECT；
5. HTTPS 再启 TLS。

---

## 7. 连接生命周期与断线重连

`MagicClient` 不再继承 `Socket`，改为持有底层传输句柄（类似 cURL handle）：

- 同一实例可跨请求复用连接；
- 连接断开后可在内部重建传输链路；
- 重连逻辑统一放在底层 `TransportClient`，高层只声明策略。

重连边界：

1. 默认仅自动重试幂等方法（`GET/HEAD/PUT/DELETE/OPTIONS`）；
2. `POST/PATCH` 默认不自动重试，除非显式开启且请求体可重放；
3. 响应体已开始消费后断流（尤其 SSE/chunked）不做透明续传，直接抛错；
4. 每次重连都保留失败阶段信息（`phase`），便于线上定位。

建议在 `MagicClient` 暴露重试选项：

- `retry.max_attempts`
- `retry.retryable_methods`
- `retry.backoff_ms`
- `retry.on_connection_lost`（仅首包前生效）

---

## 8. 异常与观测

底层异常统一附带上下文：

- `proxy_type`
- `proxy_endpoint`
- `target_endpoint`
- `phase`（`connect|auth|tunnel|tls|request|response`）
- `proxy_status`（HTTP CONNECT 失败时）
- `socks5_reply_code`（SOCKS5 失败时）

`Psr7\Client\Client` 与 `MagicClient` 只做包装，不丢失这些字段。

---

## 9. 不做兼容（一次性切换）

本方案不保守兼容旧的“手工 connect + 手工 enableCrypto + 手工 CONNECT”调用模式。

风险：

1. 旧调用方需迁移到新入口；
2. 依赖 `Client extends Socket` 细节的代码需要调整；
3. 连接复用键必须纳入代理维度，避免串线。

收益：

- 代理能力集中在一个底层模块，行为一致；
- `Psr7` 层更薄，更易维护；
- `MagicClient` 让业务调用更短更稳。

---

## 10. 实施顺序

1. 新增 `Http/Proxy/*` 与 `Http/Client/TransportClient`；
2. 迁移 `Psr7\Client\Client` 到组合模式；
3. 在 `TransportClient` 落地断线检测与可配置重连策略；
4. 新增 `Psr7\Client\MagicClient` 并接入代理配置；
5. 更新 `examples/http_client/openai_streaming.php` 增加代理调用示例；
6. 补齐代理成功/失败路径测试（HTTP proxy、SOCKS5、CONNECT、认证失败、重连边界）。

完成后，代理支持在底层一次落地，`Psr7` 再以 `MagicClient` 提供傻瓜化入口。
