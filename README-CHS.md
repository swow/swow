[English](./README.md) | 中文 | [日本語](./README-JP.md)

<h2 align="center">
<a href="https://github.com/swow/swow"><img width="320" height="86" alt="Swow Logo" src="https://docs.toast.run/assets/images/swow.svg" /></a><br />
🚀 Swow 是一个专注于并发 I/O 的跨平台协程引擎
</h2>

[![license][license-badge]][license-link]
[![ci][ci-badge]][ci-link]
[![codecov][codecov-badge]][codecov-link]
[![release][release-badge]][release-link]
![❤️][made-with-love-badge]
![php][supported-php-versions-badge]
![platform][supported-platforms-badge]
![architecture][supported-architectures-badge]

## 👾 设计理念

Swow 致力于使用最小 C 核心及多数 PHP 代码以支持 PHP 高性能网络编程，这决定了它在保障关键性能之外，更多的是通过 PHP 代码与 C 内核的无缝融合运作，提供强大的二次开发能力。

此外，它还提供了丰富多样的调试机制与强大且低门槛的调试工具，最大程度地确保开发者免受 BUG 困扰，普通开发者也能通过工具的加持具备接近专家级别的 DEBUG 能力，从而将开发效率最大化。

## ✨ 特性

**照常写阻塞风格的 PHP 代码，Swow 让它并发运行；从 2.0 起，还能用上每一个 CPU 核心。**

| | 能力 | 你将得到 |
| --- | --- | --- |
| 🧶 | **完全可控的协程** | 有栈协程 `Coroutine`，配套通道 `Channel`、等待组 `WaitGroup`、等待引用 `WaitReference`、选择器 `Selector` 与 `Defer`。任何协程都可以随时查看、中断、杀死 |
| 🪄 | **现有代码直接并发** | 包括但不限于以下调用，都会让出给其它协程而不是阻塞进程，无需修改代码：<br>**定时**：`sleep()`、`usleep()`、`time_nanosleep()`、`time_sleep_until()`<br>**DNS**：`gethostbyname()`<br>**文件与管道**：`fopen()`、`fread()`、`fwrite()`、`file_get_contents()`、`flock()` 等文件函数<br>**PHP 流**：TCP、UDP、Unix 套接字与 TLS，包括 `fsockopen()`、`stream_socket_client()`、`stream_socket_server()`、`stream_select()` 以及通过 HTTP(S) 的 `file_get_contents()`<br>**cURL**：`curl_exec()`、`curl_multi_*` 与 HTTP/2，因此 Guzzle 等基于 cURL 的 HTTP 客户端同样适用<br>**MySQL**：PDO MySQL 与 `mysqli`（经由 `mysqlnd`）<br>**PostgreSQL**：PDO PgSQL<br>**MongoDB** <sup>2.0</sup>：官方 `mongodb` 扩展<br>**Redis**：基于 PHP 流的客户端，例如 phpredis 与 Predis<br>**子进程**：`proc_open()`<br>**等等**：任何构建在以上能力之上的扩展或纯 PHP 库 |
| 🌐 | **完整的网络栈** | `Socket` 覆盖 TCP、UDP、Unix 套接字、管道与 TLS，内置 HTTP 和 WebSocket 解析器，以及用 PHP 编写的兼容 PSR-7 的 HTTP、WebSocket 服务端与客户端 |
| 🧵 | **真正的多线程** <sup>2.0</sup> | 线程 `Thread` 让 PHP 运行在系统线程上，配套跨线程通道 `Thannel`、锁 `Mutex`、信号量 `Semaphore`、`Futex`、共享内存 `SharedBuffer`，以及管理 Worker 线程的 `ThreadManager`。需要线程安全的 PHP 构建 |
| 🏭 | **全平台的多进程** <sup>2.0</sup> | `ChildProcess` 以及会重启、重载 Worker 的 `ProcessManager`，在 Windows 上同样可用 |
| 🍃 | **协程化的 MongoDB 驱动** <sup>2.0</sup> | 官方 `mongodb` 扩展的 I/O 异步化，不再阻塞进程。使用 `--enable-swow-mongodb` 构建 |
| 🐞 | **SDB（Swow Debugger）** | 用 PHP 编写的交互式调试器，命令与 gdb 相仿：列出所有协程、查看调用栈与源码、设置断点、单步执行、打印变量 |
| 🐶 | **Watchdog** | 发现长时间占用 CPU 或阻塞在系统调用中的协程。可以让它让出给其它协程，也可以交给你的回调来挂起或杀死它 |
| 🔬 | **更多调试机制** | 死锁检测与执行追踪 |

2.0 的全部新内容见 [CHANGELOG](./CHANGELOG.md)。

## 🎮 安装

> 和任何开源项目一样, Swow 总是在**最新的发行版**提供更佳的稳定性和更强大的功能, 请尽量保证你使用的是最新版本。

### 🔎 必要条件

- Linux、Windows、macOS 等常见操作系统，与 [libuv](https://github.com/libuv/libuv/blob/v1.x/SUPPORTED_PLATFORMS.md) 支持的平台基本一致
- PHP 8.0.0 或以上版本，推荐使用最新版本
- 如需使用多线程设施，需要线程安全（ZTS）的 PHP 构建

### 🥧 使用PIE安装扩展

先获取[PIE](https://github.com/php/pie)

```shell
pie install swow/swow-extension
```

develop版本（一般来说有最新的bugfix）：

```shell
pie install swow/swow-extension:dev-develop
```

**注意** 目前（2025年11月13日）腾讯云composer镜像的元信息不正确，这导致无法正常使用PIE安装，可以使用官方packagist或其他镜像尝试，见[#283](https://github.com/swow/swow/issues/283)

### 👨‍🎤 通过 Composer 安装 (扩展 + PHP 库)

拉取源码：

```shell
composer require swow/swow
```

然后运行 `vendor/bin` 目录下的自动安装器 `swow-builder` 即可开始安装：

```shell
./vendor/bin/swow-builder --install
```

当你更新了 Swow 的源码时，你最好使用 `rebuild` 选项来重新编译 Swow 再安装：

```shell
./vendor/bin/swow-builder --rebuild --install
```

更多关于 `swow-builder` 的使用方法，请参考 [安装文档 - 通过 Composer 安装](https://docs.toast.run/swow/chs/install.html#%E9%80%9A%E8%BF%87-composer-%E5%AE%89%E8%A3%85)。

### 🐧 手动编译安装 (UNIX-like)

克隆项目（也可以通过 Composer 引入，cd 到 `vendor/swow/swow/ext` 中手动编译）：

```shell
git clone https://github.com/swow/swow.git
```

熟悉的构建三板斧，最后使用root权限安装到系统：

```shell
cd swow/ext && \
phpize && ./configure && \
make clean && make -j && \
sudo make install
```

详见 [安装文档 - 手动编译安装 (UNIX-like)](https://docs.toast.run/swow/chs/install.html#%E6%89%8B%E5%8A%A8%E7%BC%96%E8%AF%91%E5%AE%89%E8%A3%85-unix-like-%E6%88%96-cygwinmsyswsl)。

### Ⓜ️ 手动编译安装 (Windows)

详见 [安装文档 - 手动编译安装 (Windows)](https://docs.toast.run/swow/chs/install.html#%E6%89%8B%E5%8A%A8%E7%BC%96%E8%AF%91%E5%AE%89%E8%A3%85-windows)。

也可在 [最新的发行版](https://github.com/swow/swow/releases/latest) 中直接下载 DLL。

## 🔰️ 安全问题

安全问题应该通过邮件私下报告给 [twosee@php.net](mailto:twosee@php.net)。
你将在 24 小时内收到回复，如果因为某些原因你没有收到回复，请通过回复原始邮件的方式跟进，以确保我们收到了你的原始邮件。

## 🖊️ 贡献

你的贡献对于 Swow 的发展至关重要！

你可以通过以下方式来贡献：

* [报告问题与反馈](https://github.com/swow/swow/issues)
* [通过 Pull Request 提交修复补丁或新的特性实现](https://github.com/swow/swow/pulls)
* [帮助我们完善文档 (通过 GitHub 贡献)](https://github.com/toastrun/docs.toast.run)

## ❤️ 贡献者

PHP 高性能协程网络通信引擎开发之路曲高和寡，这是一项高处不胜寒的开源工作。

非常感谢以下小伙伴对于 Swow 项目的代码贡献，没有大家就没有现在的 Swow。

[![Contributors](https://opencollective.com/swow/contributors.svg?width=890&button=false)](https://github.com/swow/swow/graphs/contributors)

## 💬 开发 & 讨论

- **文档**：[https://docs.toast.run/swow/chs](https://docs.toast.run/swow/chs)
- **博客** [https://docs.toast.run/swow-blog/chs](https://docs.toast.run/swow-blog/chs)
- **能力项** [https://docs.toast.run/swow-blog/chs/init.html](https://docs.toast.run/swow-blog/chs/init.html)
- **API参考**：[https://docs.toast.run/swow-api/develop/](https://docs.toast.run/swow-api/develop/)
- **讨论区**：[https://github.com/swow/swow/discussions](https://github.com/swow/swow/discussions)
- **计划列表**：[https://github.com/swow/swow/projects](https://github.com/swow/swow/projects)

## 📃 开源协议

Swow 使用 [Apache License Version 2.0](http://www.apache.org/licenses/LICENSE-2.0.html) 开源。

Swow 也包含了一些第三方库，这些库的开源协议各不相同，请参考 [LICENSES.full](LICENSES.full) 获取完整的开源协议文本。

[license-badge]: https://img.shields.io/badge/license-apache2-blue.svg
[license-link]: LICENSE
[ci-badge]: https://github.com/swow/swow/workflows/tests/badge.svg
[ci-link]: https://github.com/swow/swow/actions?query=workflow:tests
[codecov-badge]: https://codecov.io/gh/swow/swow/branch/develop/graph/badge.svg
[codecov-link]: https://codecov.io/gh/swow/swow
[release-badge]: https://img.shields.io/github/release/swow/swow.svg?include_prereleases
[release-link]: https://github.com/swow/swow/releases
[made-with-love-badge]: https://img.shields.io/badge/made%20with-%E2%9D%A4-f00
[supported-php-versions-badge]: https://img.shields.io/badge/php-8.0--8.5-royalblue.svg
[supported-platforms-badge]: https://img.shields.io/badge/platform-Win32%20|%20GNU/Linux%20|%20macOS%20|%20FreeBSD%20-gold
[supported-architectures-badge]: https://img.shields.io/badge/architecture-x86--64%20|%20ARM64%20|%20mips64el%20|%20riscv64%20-maroon
