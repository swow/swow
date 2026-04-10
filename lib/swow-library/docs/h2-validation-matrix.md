# H2 验证矩阵

## 1. 已真实跑通

以下场景已经通过真实请求验证：

1. TLS + ALPN `h2`
2. TLS + ALPN `http/1.1`
3. `h2c` upgrade 成功
4. `h2c` upgrade 失败
   - 缺失 `HTTP2-Settings`
   - 非法 `HTTP2-Settings`
5. `h2c` upgrade 请求带 body
6. request trailer
7. response trailer
8. 多 stream 并发请求
9. 小窗口下大响应恢复发送

## 2. 使用的脚本

### 2.1 运行时服务脚本

- [h2-runtime-server.php](/Users/heping/PhpWorkSpace/swow-cloud/swow/examples/http_server/h2-runtime-server.php)

支持：

- `tls` 模式
- `clear` 模式

提供接口：

- `/health`
- `/echo`
- `/small`
- `/big`
- `/trailer`
- `/goaway`

### 2.2 原始 H2 检查脚本

- [h2-runtime-checks.php](/Users/heping/PhpWorkSpace/swow-cloud/swow/examples/http_server/h2-runtime-checks.php)

支持模式：

1. `trailers`
2. `multistream`

## 3. 推荐验证顺序

### 3.1 TLS

启动：

```bash
php -d extension=swow examples/http_server/h2-runtime-server.php tls 50
```

验证：

```bash
curl -skv --http2 https://127.0.0.1:PORT/health
curl -skv --http1.1 https://127.0.0.1:PORT/health
```

### 3.2 h2c

启动：

```bash
php -d extension=swow examples/http_server/h2-runtime-server.php clear 50
```

验证：

```bash
curl -sv --http1.1 http://127.0.0.1:PORT/health
curl -sv --http2 http://127.0.0.1:PORT/health
curl -sv --http2-prior-knowledge http://127.0.0.1:PORT/health
```

### 3.3 trailer 与多 stream

```bash
php -d extension=swow examples/http_server/h2-runtime-checks.php trailers PORT
php -d extension=swow examples/http_server/h2-runtime-checks.php multistream PORT
```

## 4. 推荐继续关注的边界

1. `GOAWAY(lastStreamId)` 后旧 stream 收尾
2. `RST_STREAM` 在等待窗口恢复期间打断发送
3. `HEADERS + CONTINUATION` 被延迟后恢复
4. H2 preface 分包到达
5. even stream / `stream 0` 的 request 类帧

## 5. 当前结论

当前 H2 代码已经具备：

1. TLS + ALPN 自动分流
2. `h2c` upgrade
3. request / response trailer
4. 多 stream 请求
5. 小窗口恢复发送

如果后续继续回归，优先复用上面的两个脚本，而不是再临时拼装测试入口。
