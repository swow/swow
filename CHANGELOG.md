# v1.7.0-alpha.1

> Release Date: 2026-03-08 | 发布日期：2026-03-08

> "I finally got a chance to write code for this project." —— AI
>
> 「我终于有机会写这个项目了。」 —— AI

### Version Highlights | 版本亮点

This alpha release includes proxy support in MagicClient, new streaming/event-stream and timeout capabilities for HTTP client workflows, watchdog and closure serialization improvements, plus broad SSL/PHP compatibility updates.
该 alpha 版本包含 MagicClient 代理支持、HTTP 客户端流式与 event-stream/超时能力、watchdog 与闭包序列化改进，以及较大范围的 SSL/PHP 兼容性更新。

## 🐣 What's New | 什么是牛的

+ Add HTTP and SOCKS5 proxy support in MagicClient | 为 MagicClient 增加 HTTP 和 SOCKS5 代理支持 (49b3231e) [@twosee]
+ Add HTTP client chunked streaming and event-stream handling support | 增加 HTTP 客户端 chunked 流式与 event-stream 处理支持 (f0a832c1, af3f3f38) [@twosee]
+ Add explicit timeout parameters for PSR-7 recv entity APIs | 为 PSR-7 接收实体 API 增加显式超时参数 (aa339560) [@twosee]
+ Add `Debug\block()` and watchdog syscall blocking detection improvements | 新增 `Debug\block()` 并增强 watchdog 系统调用阻塞检测 (a2ff1f17) [@twosee]
+ Add closure serializer INI options for runtime control | 新增闭包序列化相关 INI 配置项用于运行时控制 (bec43b4d, 62a97ad8) [@dixyes]
+ Add `peer_fingerprint`, `capture_peer_cert`, and `SNI_server_certs` support in stream/SSL paths | 在流/SSL 路径新增 `peer_fingerprint`、`capture_peer_cert` 和 `SNI_server_certs` 支持 (04c7d254, 42e0328c, c46ac9a2, 4f0c886f) [@dixyes]

## ✨ What's Enhanced | 加强了啥

+ Refactor OpenAI streaming example to use MagicClient and improve proxy flow consistency | 重构 OpenAI 流式示例以使用 MagicClient，并改进代理流程一致性 (9070013c) [@twosee]
+ Improve PDO_PGSQL support detection and align multiple behaviors/warnings with PHP upstream | 改进 PDO_PGSQL 支持检测并让多处行为/告警更贴近 PHP 上游 (446e537d, 2ef7b0ad, 3fded8eb, 3c268506, de062d18) [@twosee] [@dixyes]
+ Adopt PHP 8.6 `in_autoload` changes and add compatibility for PHP < 8.2 atomic exchange | 适配 PHP 8.6 `in_autoload` 变更并增加 PHP < 8.2 原子交换兼容 (21329405, 6fe13c47) [@dixyes] [@twosee]

## 🐛 What's Fixed | 什么修了

* Fix segv when using `fsockopen` | 修复使用 `fsockopen` 时的段错误 (b0e263e3) [@dixyes]
* Fix watchdog alerter atomic operation and PHP 8.2 build issues | 修复 watchdog alerter 原子操作与 PHP 8.2 构建问题 (9780f686, 9ef06c83) [@twosee] [@dixyes]
* Fix PHP stream TLS version option handling and PIE argument issues | 修复 PHP stream TLS 版本选项处理与 PIE 参数问题 (470c572f, 5650301c) [@dixyes]
* Fix flaky watchdog tests and assertion boundary checks | 修复 watchdog 测试偶发失败与断言边界判断问题 (ca941f77, 541daf35) [@twosee]
* Fix unsafe closure serialization case for anonymous class static methods | 修复匿名类静态方法在闭包序列化场景下的不安全行为 (2e7bdb78) [@dixyes]

## 👻 What's Removed | 什么移除了

- Remove legacy exception save/restore hooks and outdated `disable_class` related logic | 移除旧异常保存/恢复钩子与过时 `disable_class` 相关逻辑 (e5994de7, c5e2ff37) [@dixyes]
- Remove cURL load-order check and other unused legacy checks | 移除 cURL 加载顺序检查及其他未使用检查 (4f9633dc, ae3265e6) [@dixyes]

## 📦 Internal | 内部

+ Sync dependencies and upstream sources/stubs from PHP/libcat | 同步依赖并更新来自 PHP/libcat 的源码与 stub (1957ea99, 9d23b0b9, d347bd84, ffe25459, 02d1bfec, f3babafc) [@dixyes]
+ Improve CI/test matrix and stabilize watchdog/SSL related tests | 改进 CI/测试矩阵并增强 watchdog/SSL 相关测试稳定性 (5015c6cd, c714d431, 57177bee, f4b643f2, b6131389) [@dixyes] [@twosee]
+ Include various compiler/style/tokenizer/pgsql list maintenance updates | 包含编译器/风格/tokenizer/pgsql 列表等维护性更新 (c03256f6, c8620a53, 9e78a18f, b07721ac, 924b511d, bdbddc9c, 11df0885) [@dixyes]

---

# v1.6.2

> Release Date: 2025-11-03 | 发布日期：2025-11-03

> "Fixing bugs is like whack-a-mole, but at least we're getting better at it" —— AI
>
> 「修 BUG 就像打地鼠，但至少我们越来越熟练了」 —— AI

### Version Highlights | 版本亮点

This version focuses on cURL stability fixes, PHP 8.5 compatibility improvements, and build system enhancements.
该版本专注于 cURL 稳定性修复、PHP 8.5 兼容性改进以及构建系统增强。

## 🐣 What's New | 什么是牛的

- Nothing new | 啥都没有新增

## ✨ What's Enhanced | 加强了啥

+ Improve PHP 8.5 compatibility with zend_disable_class support | 通过 zend_disable_class 支持改进 PHP 8.5 兼容性 (58494848) [@dixyes]
+ Change release DLL name to match PIE standards | 更改发布 DLL 名称以匹配 PIE 标准 (ec7e7681) [@dixyes]
+ Use gnu11 standard for build | 构建使用 gnu11 标准 (fe10fb3c) [@dixyes]

## 🐛 What's Fixed | 什么修了

* Fix OpenSSL .rnd file read and creation on macOS | 修复 macOS 上 OpenSSL .rnd 文件读取和创建 (232cacee, 2c4ac7b3) [@dixyes]
* Fix cURL runtime shutdown sequence | 修复 cURL 运行时关闭序列 (a69146b6) [@dixyes]
* Fix ARM64 platform INI file lookup | 修复 ARM64 平台 INI 文件查找 (e5fba705) [@dixyes]
* Fix PHP 8.5 deprecation warnings | 修复 PHP 8.5 废弃警告 (19d20413) [@dixyes]
* Revert zend_is_callable_ex wrapper removal | 回退 zend_is_callable_ex 包装器移除 (f7ae1c7e) [@dixyes]

## 👻 What's Removed | 什么移除了

- Remove PHP 8.0 from distro tests | 从发行版测试中移除 PHP 8.0 (0d39f1b5) [@dixyes]

## 📦 Internal | 内部

+ Sync libcat dependencies (multiple updates) | 同步 libcat 依赖（多次更新）(5f1eda65, 748ecc4f, 1595c397, 9051b512) [@dixyes]
+ Add comprehensive cURL hook tests for multi handle deadloop and callback exceptions | 添加 cURL 钩子测试以覆盖 multi handle 死循环和回调异常 (096024f8, 4813c1de, f5a434c2) [@dixyes]
+ Add Rocky Linux and AlmaLinux 10 CI tests | 添加 Rocky Linux 和 AlmaLinux 10 CI 测试 (684758ed) [@dixyes]
+ Update code from PHP source tree | 从 PHP 源码树更新代码 (a1031796) [@dixyes]
+ Add source update utility | 添加源码更新工具 (14615871) [@dixyes]
+ Update certificate generator with EC private key support | 更新证书生成器，支持 EC 私钥 (ef45c3a0, b80e55de) [@dixyes]
+ Patch phpize.js for newer wsh compatibility | 修补 phpize.js 以兼容新版 wsh (38263e88) [@dixyes]
+ Various test improvements for platform-specific issues | 针对平台特定问题的各种测试改进 (e103e3df, 93c0086b, fa38bf55) [@dixyes]

---

# v1.6.1

> Release Date: 2025-08-10 | 发布日期：2025-08-10

> "The next version will be better" —— Swow Team
>
> 「下一个版本会更好」 —— Swow Team

### Version Highlights | 版本亮点

This version focuses on stability improvements, PIE (PHP Installer for Extensions) support, cURL reliability fixes, and PostgreSQL compatibility.
该版本专注于修复 cURL Hook、PIE (PHP Installer for Extensions) 支持、以及 PostgreSQL 兼容性。

## 🐣 What's New | 什么是牛的
+ PIE (PHP Installer for Extensions) support and installation documentation | PIE (PHP Installer for Extensions) 支持和安装文档 (f161c4bf, b20583d3) [@dixyes]

## ✨ What's Enhanced | 啥叫enhance?

+ Use warning instead of exception for extension version mismatch | 扩展版本不匹配使用警告而非异常 (8f311ec6) [@twosee]

## 🐛 What's Fixed | 什么修了

* Fix cURL hook implementation | 修复 cURL 钩子实现 (10a0dbb8) [@dixyes]
* Fix cURL module initialization | 修复 cURL 模块初始化 (6a828238) [@dixyes]
* Fix PQclosePrepared weak dependency | 修复 PQclosePrepared 弱依赖 (bb348b9d) [@dixyes]
* Fix missing weak dependencies | 修复缺失的弱依赖 (2f7f44d1) [@dixyes]

## 👻 What's Removed | 什么移除了

- Nothing removed | 啥都没有移除

## 📦 Internal | 内部

+ Improved cURL multi test to cover blocking issues | 改进 cURL 多重测试以覆盖阻塞问题 (5e592cb0) [@twosee]
+ Match PHP cURL minfo | 匹配 PHP cURL minfo (50d0e87d) [@dixyes]

---

# v1.6.0

> Release Date: 2025-08-02 | 发布日期：2025-08-02

> "This version did a lot of things" —— Swow Team
>
> 「该版本做了许多事」 —— Swow Team

### Version Highlights | 版本亮点

This version provides full support for PHP 8.4, updated more powerful features, enhanced SSL/TLS reliability, critical memory security fixes, and significant architectural improvements.
该版本提供了完整的 PHP 8.4 支持、更新更强大的功能、增强的 SSL/TLS 可靠性、关键的内存安全修复以及重大的架构改进。

## 🐣 What's New | 什么是牛的

+ Full PHP 8.4 support | PHP 8.4 完整支持 (754a868, ee976a5) [@dixyes]
+ New pipe stuff: `Swow\pipe()`, `Swow\fileno()`, `Swow\pipe_from_fd()` | 新增管道 API (627fe16) [@dixyes]
+ Add `Swow\nproc()` to get CPU core count | 新增 `Swow\nproc()` 获取 CPU 核心数 [@dixyes]
+ EventDriver gets `stop()` method for graceful shutdown | EventDriver 新增 `stop()` 方法用于优雅停止服务器 (#269) [@devhaozi]
+ New INI: `swow.hook_pdo_pgsql` | 新 INI：`swow.hook_pdo_pgsql` (70af3d5) [@dixyes]

## ✨ What's Enhanced | 啥叫enhance?

+ Complete closure serialization rewrite (⚠️ Breaking Change) | 闭包序列化完全重构（⚠️ 破坏性变更）
  - Use AST to build closure code | 用 AST 构建闭包代码 (d5fd9ba, 92154a3, 9d0f796) [@dixyes]
+ Performance stuff: Apple Clang compatibility, libcurl bump to 7.61.0 | 性能优化：Apple Clang 兼容性，libcurl 最低版本提升到 7.61.0 (42096dc, a264539) [@twose]

## 🐛 What's Fixed | 什么修了

* Fix SSL problems: `enableCrypto()` and stream CA read | 修了 SSL 的一堆问题：`enableCrypto()` 和流 CA 读取 [@dixyes]
* Fix Use-After-Free bugs in coroutine stuff and exception handling | 修复 Use-After-Free：协程管理和异常处理中的内存安全问题 (d2a549d, 3993aeb) [@dixyes]
* Fix UAF when killing coroutines and throwing exceptions | 修复协程终止和异常抛出时的 UAF (ec8c6c6) [@dixyes]
* Fix ZTS build crash on shutdown | 修复 ZTS 构建关闭时的崩溃 (ec8c6c6) [@dixyes]
* Fix many PHP 8.4 compatibility problems | 修复 PHP 8.4 各种兼容性问题 (0c09661, 7e68033, 6fa98d8) [@dixyes]
* Update libpq search paths | 更新 libpq 查找路径 [@dixyes]
* Fix build stuff: string terminators, left shift overflow, macro problems etc. | 修复构建问题：字符串终止符、左移溢出、宏展开等 (7cf2cf6, bdda08b, ccf437f, db35d83, d39500f) [@dixyes]
* Fix undefined Content-Length error in HTTP responses | 修复 HTTP 响应 Content-Length 未定义错误 (09f5c65) [@twose]

## 👻 What's Removed | 什么移除了

- Remove `Swow\Errno::ESTALE` | 移除 `Swow\Errno::ESTALE` [@dixyes]

## 📦 Internal | 内部

+ Update libcat's libuv | 更新 libcat 的 libuv (@twose)
+ Optimize many compatibility macros or wrappers | 优化许多兼容性宏或包装 (@dixyes)
+ Update swow_fs from PHP source tree for better file system handling | 从 PHP 源码树更新 swow_fs 以改进文件系统处理 (91e9758, 4b32c71) [@dixyes]
+ Update to latest PHP 8.4 PostgreSQL source code | 更新到最新 PHP 8.4 PostgreSQL 源码，增强各种能力 (da3562a, d7736c4, 0ed3782) [@dixyes]
+ Add tests: OpenSSL hooks, TLS, Property Hooks | 新增各种测试：OpenSSL 钩子、TLS、Property Hooks (c046201, 1dde6d9, 9efeaf7) [@dixyes]
+ Better CI/CD configs and dev tools | 增强 CI/CD 配置和开发工具 (60fdf81, f7dff44, dc8e9e1, 48a6555, 8fe564f, 62758f7, c2b442b, 9850f7e) [@dixyes]

---

# v1.5.3

> release-date: 2024-07-22

> 「成功的关键在于我们对失败的反应。」 - ChatGPT
>
> "The key to success is how we respond to failure." - ChatGPT

该版本主要修复了 cURL 在特定条件下会死循环的问题，并新增了日语版本的 README。

This version mainly fixes the issue of cURL getting stuck in an infinite loop under certain conditions, and adds a Japanese version of the README.

## 🐣 What's New

+ Add FileSystem::scanDirRecursive() (b238947) (@twose)

## ✨ What's Enhanced

+ Japanese README (#252) (@eltociear)

## 🐛 What's Fixed

* Fix cURL dead loop bug (libcat/libcat@0d68189e) (@twose)
* Fix strange hard code in builder (0fcf143) (@twose)

## 👻 What's Removed

- Nothing removed

## 📦 Internal

+ Update swow_fs (44621bb) (@dixyes)

---

# v1.5.2

> release-date: 2024-05-13

> 真正的快乐不是没有痛苦，而是能在痛苦中找到乐趣。 - ChatGPT
>
> True joy is not about being without pain, but about finding pleasure in pain. - ChatGPT

这是一个修复版本。

## 🐣 What's New

+ Nothing new

## ✨ What's Enhanced

+ Nothing enhanced

## 🐛 What's Fixed

* Callable curl options should be nullable (682b4be) (@twose)

## 👻 What's Removed

- Nothing removed

---

# v1.5.1

> release-date: 2024-05-13

> 「没有最终的成功，也没有致命的失败，最可贵的是继续前进的勇气。」 - 温斯顿·丘吉尔
>
> "Success is not final, failure is not fatal. It is the courage to continue that counts." - Winston Churchill

这是一个修复版本。

## 🐣 What's New

+ Nothing new

## ✨ What's Enhanced

+ Nothing enhanced

## 🐛 What's Fixed

* Fix flock on Windows (e1121c0) (@dixyes)
* Fix missing CURLM_RECURSIVE_API_CALL on curl < 7.59.0 (a92d70c) (@dixyes)
* Fix wrong parameter position of recvMessage() call (#240) (@assert6)
* Fix FCC implementation compatibility (46e7d3d) (@twose)
* Fix slow cURL when composer install (libcat/libcat@55959bf) (@twose)
+ Fix incorrect nNumUsed of functions table (07cea2b) (@twose)

## 👻 What's Removed

- Nothing removed

---

# v1.5.0

> release-date: 2024-05-05

> 「优秀的项目获取 star，伟大的项目赢得人心。」 - Swow
>
> "Excellent projects earn stars, great projects win hearts and minds." - Swow

版本亮点：

1、全新的 cURL 协程化支持，底层重构实现，覆盖各种边缘 cases；

2、完整包含 PHP cURL 实现，不再依赖 cURL 扩展，因此可适配所有运行环境；

3、Debugger 支持远程 telnet 调试、Psr7 Server 支持 HTTP chunked response。

## 🐣 What's New

+ Brand new cURL full support!!! (9a44716) (@twose)
+ Debugger support EOF Stream now! (2710c0e) (@twose)
+ Support send HTTP chunked response (b185524) (@twose)
+ Add Context and CoroutineContext (a16429d) (@twose)
+ Support PHP 8.3 and PHP 8.4-dev (@twose)

## ✨ What's Enhanced

+ Add EventDriver example for HTTP Server (8521677) (@twose)
+ Show server url in mixed server example (0520690) (@twose)
+ Introduce polyfill file to fix PHP-8.4-dev deprecations (bf47c99) (@twose)

## 🐛 What's Fixed

* Fix WebSocket parsing bug and add a test for it (bd9b93d) (@twose)
* Fix missing socket crypto options (99c502b) (@twose)
* Fix #235 (SSLv3 compilation error) (9e2c0fb) (@twose)
* Fix builder error when php-config is not configured (93fd794) (@twose)
* Fix example code (41da6e8) (@twose)
+ Use maxHeaderLength for Buffer size and Fix construction order (9ac0e70) (@twose)
* Fix errors in tools (6243bb3) (@twose)

## 👻 What's Removed

- We are no longer rely on cURL extension (e462ce2) (@twose)
- Remove unused clean_module_class_callback (a53184d) (@twose)
- Remove unused maxBufferSize things (d38ddce) (@twose)

## 📦 Internal

+ Some SSL fixes in libcat (@dixyes)
+ Update swow_fs from php (b5c9654) (@dixyes)
+ Update swow_fs rev anchors (49a91cc) (@dixyes)
+ Update artifact actions (de65d57) (@dixyes)
+ Support PHPUnit 11 (@twose)
+ Support hook constants and cleanup modules (34ad07c) (@twose)
+ Make blank-fixer smarter (177cbd0) (@twose)
+ Fix -ssl not found error on macOS CI (57a0baa) (ccd28c2) (@twose)

----

# v1.4.1

> release-date: 2023-12-10

> 「生活并不是等待暴风雨过去，而是学会如何在雨中跳舞。」- 维维安·格林
>
> "Life isn't about waiting for the storm to pass. It's about learning how to dance in the rain." - Vivian Greene

## 🐛 What's Fixed

* Fix SSL context not work (fb15f29) (libcat/libcat@bc6ec0a8) (@twose)
* Initialize in_autoload in runtime init (8fcdbd0) (@dixyes)
* Fix missing stream error info (232b509) (@twose)

---

# v1.4.0

> release-date: 2023-12-03

> 「不要盯着时钟看，要做时钟所做的事情，继续前进。」 - 萨姆·莱文森
>
> "Don't watch the clock; do what it does. Keep going." - Sam Levenson

## 🐣 What's New

+ Add EventDriver for Psr7 Server (54cd3ab) (@twose)
+ New API: Socket->open() (6bbcd89) (@twose)
+ New API: stream_select_unlimited (231f580) (@twose)
+ Support SSL security_level and alpn_protocols (8fdda59) (@twose)
+ Support serialization for Buffer (206fb1b) (@twose)
+ Support coroutine switch in autoload (f49445b) (@twose)
+ Add lo_lseek64 weak symbol (f7cefee) (@dixyes)
+ Add --enable-debug-log to the compilation options (ce6fb89) (@twose)
+ Support disable Swow by `swow.enable=0` (7f46b5e) (@twose)

## ✨ What's Enhanced

+ Enhance showExecutedSourceLines() (addd0de) (@twose)
+ Show SSL info in stream metadata (c12d1c9) (@twose)
+ Improve `php --ri swow` output (393304f) (@twose)
+ Make parsedBody nullable (#202) (@limingxinleo)
+ Support v2.0 for `psr/http-message` (#199) (@limingxinleo)
+ Handle SIGHUP signal in dontdie (#225) (@AuroraYolo)
+ Add return type to recvMessage method (#224) (@AuroraYolo)
+ Update namespace in stream tests (#226) (@AuroraYolo)
+ Make php-wrapper smarter (5e8fc64) (@twose)
+ Use GPT to generate CHANGELOG (4f250e7) (@twose)

## 🐛 What's Fixed

* Workaround for mysql_handle_closer() (56d6ffe) (@twose)
* Fix socket_export_stream() hook (f6e6b28) (f94d381) (@twose)
* Fix cURL on PHP-8.3 (76a5bcc) (@twose)
* Fix #208 (ipv6_v6only compatibility) (d2059ad) (@twose)
* Fix return value of stream_socket_sendto() (e1f6a7f) (@twose)
* Fix weak dep symbols (b97bd1e) (@dixyes)

## 👻 What's Removed

- Remove interface dependencies from psr7-plus (a990bf2) (@twose)

---

# v1.3.1

> release-date: 2023-06-15

> 「成就伟大的唯一途径是热爱自己的工作。」 - 史蒂夫·乔布斯
>
> "The only way to do great work is to love what you do." - Steve Jobs

## 🐛 What's Fixed

* Fix memory error due to call destructor in scheduler (#198) (@twose)

---

# v1.3.0

> release-date: 2023-06-09

> 差距并不可怕，可怕的是因为差距而放弃。
>
> The gap is not terrible. What's terrible is giving up because of it.

## 🐣 What's New

+ Support PDO PostgreSQL (#137) (@huanghantao)
+ Weak dependency for PDO (#171) (@dixyes)
+ Support ServerConnection->sendHttpFile() (#168) (@PandaLIU-1111)
+ Support remove temp file automatically (#172) (@PandaLIU-1111)
+ Support simple HTTP recvMessage timeout (dd615e9) (@twose)
+ Server config connection recvMessage timeout (#194) (@xuanyanwow)
+ Provides psr7-plus in an independent composer package (6142114)  (2667931) (@twose)
+ Introduce dontdie as an independent composer package (6ce662e) (@twose)

## ✨ What's Enhanced

+ Use psr/http-message v1.1 (e6743e5) (@twose)
+ Rename swow.async_stdio to swow.async_tty (7283708) (@twose)
+ Remove extra export ignores (2e5abe9) (@dixyes)
+ Support resolve path list of autoload in composer.json (90e0d39) (@twose)
+ Support maxExecutionTime for dontDie() (ddfe1ad) (@twose)
+ Support nickname option for dontdie (8f347af) (@twose)
+ Add a test for empty Content-Type (e6a7ce1) (@twose)
+ Added ServerConnection SendHttpFile test case (#170) (@PandaLIU-1111)
+ Add comments for some Buffer methods (57b2121) (@twose)
+ Fix "Doc tag without variable name" (ea620d9) (@twose)

## 🐛 What's Fixed

* Fix Psr7 BufferStream implementation and add tests for it (#192) (@twose)
* Fix setParsedBody params type (#174) (@duxphp)
* Fix PHP stream create TLS server error (#187) (f758a40) (@twose)
* Fix incorrect parsedOffset (94b689c) (@twose)
* Fix preserveBodyData not work (4fb20dd) (@twose)
* Fix incorrect error code of try APIs (572278b) (@twose)
* Fix require package name (c9a5dea) (@twose)
* Fix pdo header check (255ac36) (@dixyes)
* Fix ulimit typo (a628513) (@dixyes)
* Fix MODULES_CHECK_PRE memory error (90bb701) (@twose)
* Fix exdev copy logic (70cb109) (@dixyes)
* Fix Socket->sendFile() behaviour with length 0 (aad9911) (@twose)
* Always backup native ops/wrapper (efc2f80) (@twose)
* Defensive programming for ENOTCONN when call getpeername() on accepted socket (32c1fa5) (@twose)
* Defensive programming for preserve body data case (0e2cb0f) (@twose)
* Workaround for test-extension unexpected exit (70c1744) (@twose)

## 👻 What's Removed

- Nothing removed

---

# v1.2.0

> release-date: 2023-02-26

> 成功的秘诀是开始行动。
>
> The secret of getting ahead is getting started.

## 🐣 What's New

- New API: Socket->sendFile() (da2397f)
- New APIs: `Coroutine->getStartTime()`, `Coroutine->getEndTime()` (64f516e)
- New Constant: WebSocket::DEFAULT_MASKING_KEY (72f1920)
- New Config: Introduce autoUnmask config for specific requirements (56088cc)
- Support use PHP ini to configure async stdio/file hook and async threads (swow.async_threads=4, swow.async_file=1, swow.async_stdio=1 by default) (0d94d1a)

## ✨ What's Enhanced

- Support get php binary files via php-config (#158) (@sy-records)
- Run update docs ci under swow organization only (#159) (@sy-records)
- Handle empty maskingKey and empty data for WebSocket::unmask() (3d07a94)
- Skip NULL values when pack headers (977c54c)
- Add HTTP event-stream case for mixed server example (923e69e)

## 🐛 What's Fixed

- Fix stream_select() hangs on server for all platforms (#162)
- Fix missing SSL module init (90ea96c)
- Fix test-extension.php can not run without composer (329d49d)
- Fix callable memory leak when coroutine allocation failed (25be041)
- Some fixes for Debugger: Fix `p` and `vars` does not work; Fix missing step_in command (63a9c84)
- Fix PHP rename() did not work for cross devices (9903049)
- Fix WebSocket payloadData buffer size (ee8bc7c)
- Fix Message->addHeader() and add a test for it (2e51848)

## 👻 What's Removed

- Remove API: Socket->setTcpAcceptBalance() (72e7c44)

---

# v1.1.0

> release-date: 2023-01-18

> 永远不要熄灭心中的火，哪怕别人只看见烟。
> Never extinguish the fire in your heart, even if others can only see smoke.

## 🐣 What's New

- New README (b121caa)
- New PHP API: `Coroutine->getSwitches()`, `Coroutine::getGlobalSwitches()` (2ef2ac3)
- Add `HttpParser` errno (3365aaa) (0a8c96b)
- New C APIs: version getters (f0ce087)

## ✨ What's Enhanced

- Do not always close all connections when closing Server (graceful shutdown)] (dd00372)
- Optimizations for swow-builder (2ed5f60)

## 🐛 What's Fixed

- Fix bad arg name of Signal methods (6041422)

## 🐞 What's Fixed in Dependencies

- Fix va_args memory error (libcat/libcat@6d08909)
- Fix bad usage of buffer value (libcat/libcat@6c20f9e)
- Fix curl easy handle exec dead loop with SSL connection (libcat/libcat@6f5865d)
- Full cURL workaround solution for SSL connection (libcat/libcat@1e8d176)

## 👻 What's Removed

- Remove `Coroutine->getRound()`, `Coroutine::getCurrentRound()` (2ef2ac3)

---

# v1.0.0

> release-date: 2023-01-03

**Swow 是一个专注于并发 I/O 的跨平台协程引擎**，它致力于使用最小 C 核心及多数 PHP 代码以支持 PHP 高性能网络编程，具有极佳的扩展性与强大的调试能力，最大化开发者的编程效率。

Swow is a high-performance pure coroutine network communication engine, which is based on PHP and C.
It is committed to using the smallest C core and most of the PHP code to support PHP high-performance network programming.
Furthermore, it has excellent scalability and powerful debugging capabilities to maximize the programming efficiency of developers.

[Blog Post about v1.0.0](https://docs.toast.run/swow-blog/chs/init.html)
