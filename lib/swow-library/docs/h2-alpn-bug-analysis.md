# H2 ALPN Bug 分析

## 1. 问题现象

在真实 TLS 握手验证过程中，Swow socket client 开启 ALPN 时失败，报错如下：

```text
Swow\SocketException: Socket enable crypto failed, reason: SSL_CTX_set_alpn_protos() failed
```

这个问题会阻塞新加的 `Swow\Socket::getNegotiatedAlpnProtocol()` 在真实 client/server 握手中的验证，即使该 PHP 方法本身已经在运行时可见。

## 2. 复现路径

这个 bug 通过一条直接的 Swow TLS client/server 握手链路复现，配置如下：

- server ALPN: `h2,http/1.1`
- client ALPN: `h2`

相关测试入口：

- [get_negotiated_alpn_protocol.phpt](/Users/heping/PhpWorkSpace/swow-cloud/swow/ext/tests/swow_socket/get_negotiated_alpn_protocol.phpt#L25)

关键配置位置：

- server 侧带 ALPN 的 `enableCrypto()`：
  - [get_negotiated_alpn_protocol.phpt](/Users/heping/PhpWorkSpace/swow-cloud/swow/ext/tests/swow_socket/get_negotiated_alpn_protocol.phpt#L26)
- client 侧带 ALPN 的 `enableCrypto()`：
  - [get_negotiated_alpn_protocol.phpt](/Users/heping/PhpWorkSpace/swow-cloud/swow/ext/tests/swow_socket/get_negotiated_alpn_protocol.phpt#L38)

## 3. 根因分析

### 3.1 ALPN wire format 编码错误

PHP 层对外暴露的配置是人类可读的逗号分隔字符串：

```text
h2,http/1.1
```

OpenSSL **不能**直接接受这个格式。它要求的是带长度前缀的 wire format：

```text
[len]["h2"][len]["http/1.1"]
```

原来的 `cat_ssl_alpn_protos_parse()` 实现没有正确构造这个 buffer，同时在偏移和长度处理上也存在问题，可能生成无效输出。

相关代码：

- 解析入口：
  - [cat_ssl.c](/Users/heping/PhpWorkSpace/swow-cloud/swow/ext/deps/libcat/src/cat_ssl.c#L389)
- 修正后的编码循环：
  - [cat_ssl.c](/Users/heping/PhpWorkSpace/swow-cloud/swow/ext/deps/libcat/src/cat_ssl.c#L409)

### 3.2 OpenSSL 返回值语义判断反了

`SSL_CTX_set_alpn_protos()` 的返回值语义是：

- `0` 表示成功
- 非 `0` 表示失败

旧逻辑把 `0` 当成失败，所以即使 ALPN buffer 本身正确，也会被错误地报告为失败。

相关代码：

- 修正后的成功/失败判断：
  - [cat_ssl.c](/Users/heping/PhpWorkSpace/swow-cloud/swow/ext/deps/libcat/src/cat_ssl.c#L455)

### 3.3 client 路径使用了错误的缓冲区变量

解析完成后，正确的编码结果存在局部变量 `alpn` 中，但 client 侧调用 OpenSSL 时，使用的是 `context->alpn`，而不是已经解析完成的局部值。

这会导致握手代码把过期的或未正确初始化的 ALPN 数据传给 OpenSSL。

相关代码：

- 局部解析缓冲区创建位置：
  - [cat_ssl.c](/Users/heping/PhpWorkSpace/swow-cloud/swow/ext/deps/libcat/src/cat_ssl.c#L447)
- client 侧改为使用局部 `alpn` 的 OpenSSL 调用：
  - [cat_ssl.c](/Users/heping/PhpWorkSpace/swow-cloud/swow/ext/deps/libcat/src/cat_ssl.c#L455)

## 4. 修复说明

### 4.1 修正 libcat 的 ALPN 解析逻辑

修改位置：

- [cat_ssl.c](/Users/heping/PhpWorkSpace/swow-cloud/swow/ext/deps/libcat/src/cat_ssl.c#L389)

修复内容：

1. 将逗号分隔的 ALPN 字符串正确转换成 OpenSSL 所需的 wire format。
2. 拒绝空协议名、尾部分隔符以及长度超过 255 字节的协议名。
3. 将正确编码后的长度写入 `alpn->length`。

### 4.2 修正 client 侧 OpenSSL 调用的成功条件

修改位置：

- [cat_ssl.c](/Users/heping/PhpWorkSpace/swow-cloud/swow/ext/deps/libcat/src/cat_ssl.c#L452)

修复内容：

1. `SSL_CTX_set_alpn_protos()` 只有返回 `0` 时才视为成功。
2. 直接把本地编码后的 `alpn` buffer 传给 OpenSSL。

### 4.3 补充 PHP 层配置契约说明

修改位置：

- [swow_socket.c](/Users/heping/PhpWorkSpace/swow-cloud/swow/ext/src/swow_socket.c#L662)

修复内容：

1. 增加英文注释，明确 PHP 层仍然传逗号分隔字符串。
2. 明确说明把它转换成 OpenSSL wire format 的责任在 libcat。

## 5. 运行时验证结果

修复并重新编译扩展后，下面这组真实 TLS/ALPN 矩阵已经通过：

```text
h2:client=h2
h2:server=h2
http1:client=http/1.1
http1:server=http/1.1
null:client=null
null:server=null
```

这说明：

1. `h2` 协商正常。
2. `http/1.1` 协商正常。
3. 不带 ALPN 的 TLS 握手会返回 `null`。

同时也确认了新 getter 在运行时可见：

- `method_exists("Swow\\Socket", "getNegotiatedAlpnProtocol") === true`

## 6. 影响范围

这个修复只影响 **显式配置了 `alpn_protocols` 的 TLS 连接**。

它 **不会**改变：

1. 非 TLS socket
2. 未配置 ALPN 的 TLS socket
3. negotiated ALPN 处理以外的 PHP 层 H2 路由逻辑

## 7. 回归关注点

后续如果继续改 TLS 或 OpenSSL 相关代码，至少要持续检查这些场景：

1. `alpn_protocols => 'h2'`
2. `alpn_protocols => 'h2,http/1.1'`
3. `alpn_protocols => 'http/1.1'`
4. 未配置 ALPN
5. 非法值：
   - 空字符串
   - 尾部分隔符，例如 `h2,`
   - 头部分隔符，例如 `,h2`
   - 长度超过 255 字节的协议名

## 8. 相关文件

- getter 实现：
  - [swow_socket.c](/Users/heping/PhpWorkSpace/swow-cloud/swow/ext/src/swow_socket.c#L269)
- PHP 层 ALPN 选项透传：
  - [swow_socket.c](/Users/heping/PhpWorkSpace/swow-cloud/swow/ext/src/swow_socket.c#L662)
- ALPN 解析与 OpenSSL 接入：
  - [cat_ssl.c](/Users/heping/PhpWorkSpace/swow-cloud/swow/ext/deps/libcat/src/cat_ssl.c#L389)
- 运行时 PHPT：
  - [get_negotiated_alpn_protocol.phpt](/Users/heping/PhpWorkSpace/swow-cloud/swow/ext/tests/swow_socket/get_negotiated_alpn_protocol.phpt#L25)
