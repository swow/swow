[English](./README.md) | [简体中文](./README-CHS.md) | 繁體中文 | [日本語](./README-JP.md) | [Português](./README-PT.md) | [Русский](./README-RU.md)

<h2 align="center">
<a href="https://github.com/swow/swow"><img width="320" height="86" alt="Swow Logo" src="https://docs.toast.run/assets/images/swow.svg" /></a><br />
🚀 Swow 是一個專注於並行 I/O 的跨平台協程引擎
</h2>

[![license][license-badge]][license-link]
[![ci][ci-badge]][ci-link]
[![codecov][codecov-badge]][codecov-link]
[![release][release-badge]][release-link]
![❤️][made-with-love-badge]
![php][supported-php-versions-badge]
![platform][supported-platforms-badge]
![architecture][supported-architectures-badge]

## 👾 設計理念

Swow 致力於使用最小 C 核心及多數 PHP 程式碼以支援 PHP 高效能網路程式設計，這決定了它在保障關鍵效能之外，更多的是透過 PHP 程式碼與 C 核心的無縫融合運作，提供強大的二次開發能力。

此外，它還提供了豐富多樣的除錯機制與強大且低門檻的除錯工具，最大程度地確保開發者免受 BUG 困擾，一般開發者也能透過工具的加持具備接近專家級別的 DEBUG 能力，從而將開發效率最大化。

## ✨ 特性

**照常寫阻塞風格的 PHP 程式碼，Swow 讓它並行執行；從 2.0 起，還能用上每一個 CPU 核心。**

| | 能力 | 你將得到 |
| --- | --- | --- |
| 🧶 | **完全可控的協程** | 有堆疊協程 `Coroutine`，搭配通道 `Channel`、等待群組 `WaitGroup`、等待參照 `WaitReference`、選擇器 `Selector` 與 `Defer`。任何協程都可以隨時檢視、中斷、終止 |
| 🪄 | **現有程式碼直接並行** | 包括但不限於以下呼叫，都會讓出給其它協程而不是阻塞行程，無需修改程式碼：<br>**定時**：`sleep()`、`usleep()`、`time_nanosleep()`、`time_sleep_until()`<br>**DNS**：`gethostbyname()`、`gethostbynamel()`、`gethostbyaddr()`、`dns_get_record()`、`dns_get_mx()`、`dns_check_record()`<br>**檔案與管道**：`fopen()`、`fread()`、`fwrite()`、`file_get_contents()`、`flock()` 等檔案函式<br>**PHP 串流**：TCP、UDP、Unix Socket 與 TLS，包括 `fsockopen()`、`stream_socket_client()`、`stream_socket_server()`、`stream_select()`、`stream_socket_pair()` 以及透過 HTTP(S) 的 `file_get_contents()`<br>**cURL**：`curl_exec()`、`curl_multi_*` 與 HTTP/2，因此 Guzzle 等基於 cURL 的 HTTP 用戶端同樣適用<br>**MySQL**：PDO MySQL 與 `mysqli`（經由 `mysqlnd`）<br>**PostgreSQL**：PDO PgSQL，需以 `--enable-swow-pdo-pgsql` 建置<br>**MongoDB** <sup>2.0</sup>：官方 `mongodb` 擴充<br>**Redis**：基於 PHP 串流的用戶端，例如 phpredis 與 Predis<br>**子行程**：`proc_open()`，在類 UNIX 系統上還有 `popen()`、`exec()`、`shell_exec()`、`system()`、`passthru()`、`pcntl_wait()` 與 `pcntl_waitpid()`<br>**ext/sockets**：類 UNIX 系統上的 `socket_accept()`、`socket_connect()`、`socket_read()`、`socket_write()`、`socket_select()` 等阻塞呼叫<br>**等等**：任何建構在以上能力之上的擴充或純 PHP 函式庫<br>**目前尚未涵蓋**：`mail()`、`pcntl_waitid()`，以及自行進行網路 I/O 的擴充，如 `pgsql`、`ftp`、`ldap`、`snmp`，還有 SQLite 的鎖等待 |
| 🌐 | **完整的網路堆疊** | `Socket` 涵蓋 TCP、UDP、Unix Socket、管道與 TLS，內建 HTTP 和 WebSocket 解析器，以及用 PHP 撰寫的相容 PSR-7 的 HTTP、WebSocket 伺服器端與用戶端 |
| 🧵 | **真正的多執行緒** <sup>2.0</sup> | 執行緒 `Thread` 讓 PHP 執行在系統執行緒上，搭配跨執行緒通道 `Thannel`、鎖 `Mutex`、號誌 `Semaphore`、`Futex`、共享記憶體 `SharedBuffer`，以及管理 Worker 執行緒的 `ThreadManager`。需要執行緒安全的 PHP 建置 |
| 🏭 | **全平台的多行程** <sup>2.0</sup> | `ChildProcess` 以及會重啟、重新載入 Worker 的 `ProcessManager`，在 Windows 上同樣可用 |
| 🍃 | **協程化的 MongoDB 驅動** <sup>2.0</sup> | 官方 `mongodb` 擴充的 I/O 非同步化，不再阻塞行程。使用 `--enable-swow-mongodb` 建置 |
| 🐞 | **SDB（Swow Debugger）** | 用 PHP 撰寫的互動式除錯器，命令與 gdb 相仿：列出所有協程、檢視呼叫堆疊與原始碼、設定中斷點、單步執行、列印變數 |
| 🐶 | **Watchdog** | 發現長時間佔用 CPU 或阻塞在系統呼叫中的協程。可以讓它讓出給其它協程，也可以交給你的回呼來暫停或終止它 |
| 🔬 | **更多除錯機制** | 死結偵測與執行追蹤 |

2.0 的全部新內容見 [CHANGELOG](./CHANGELOG.md)。

## 🎮 安裝

> 和任何開源專案一樣，Swow 總是在**最新的發行版**提供更佳的穩定性和更強大的功能，請盡量確保你使用的是最新版本。

### 🔎 必要條件

- Linux、Windows、macOS 等常見作業系統，與 [libuv](https://github.com/libuv/libuv/blob/v1.x/SUPPORTED_PLATFORMS.md) 支援的平台基本一致
- PHP 8.0.0 或以上版本，建議使用最新版本
- 如需使用多執行緒設施，需要執行緒安全（ZTS）的 PHP 建置

### 🥧 使用 PIE 安裝擴充

先取得 [PIE](https://github.com/php/pie)

```shell
pie install swow/swow-extension
```

develop 版本（一般來說有最新的 bugfix）：

```shell
pie install swow/swow-extension:dev-develop
```

### 👨‍🎤 透過 Composer 安裝（擴充 + PHP 函式庫）

拉取原始碼：

```shell
composer require swow/swow
```

然後執行 `vendor/bin` 目錄下的自動安裝器 `swow-builder` 即可開始安裝：

```shell
./vendor/bin/swow-builder --install
```

當你更新了 Swow 的原始碼時，最好使用 `rebuild` 選項來重新編譯 Swow 再安裝：

```shell
./vendor/bin/swow-builder --rebuild --install
```

更多關於 `swow-builder` 的使用方法，請參考 [安裝文件（簡體中文）- 透過 Composer 安裝](https://docs.toast.run/swow/chs/install.html#%E9%80%9A%E8%BF%87-composer-%E5%AE%89%E8%A3%85)。

### 🐧 手動編譯安裝 (UNIX-like)

複製專案（也可以透過 Composer 引入，cd 到 `vendor/swow/swow/ext` 中手動編譯）：

```shell
git clone https://github.com/swow/swow.git
```

熟悉的建置三步驟，最後使用 root 權限安裝到系統：

```shell
cd swow/ext && \
phpize && ./configure && \
make clean && make -j && \
sudo make install
```

詳見 [安裝文件（簡體中文）- 手動編譯安裝 (UNIX-like)](https://docs.toast.run/swow/chs/install.html#%E6%89%8B%E5%8A%A8%E7%BC%96%E8%AF%91%E5%AE%89%E8%A3%85-unix-like-%E6%88%96-cygwinmsyswsl)。

### Ⓜ️ 手動編譯安裝 (Windows)

詳見 [安裝文件（簡體中文）- 手動編譯安裝 (Windows)](https://docs.toast.run/swow/chs/install.html#%E6%89%8B%E5%8A%A8%E7%BC%96%E8%AF%91%E5%AE%89%E8%A3%85-windows)。

也可在 [最新的發行版](https://github.com/swow/swow/releases/latest) 中直接下載 DLL。

## 🔰️ 安全問題

安全問題應該透過電子郵件私下回報給 [twosee@php.net](mailto:twosee@php.net)。
你將在 24 小時內收到回覆，如果因為某些原因你沒有收到回覆，請以回覆原始郵件的方式跟進，以確保我們收到了你的原始郵件。

## 🖊️ 貢獻

你的貢獻對於 Swow 的發展至關重要！

你可以透過以下方式來貢獻：

* [回報問題與意見](https://github.com/swow/swow/issues)
* [透過 Pull Request 提交修復補丁或新的特性實作](https://github.com/swow/swow/pulls)
* [幫助我們完善文件（透過 GitHub 貢獻）](https://github.com/toastrun/docs.toast.run)

## ❤️ 貢獻者

PHP 高效能協程網路通訊引擎開發之路曲高和寡，這是一項高處不勝寒的開源工作。

非常感謝以下夥伴對於 Swow 專案的程式碼貢獻，沒有大家就沒有現在的 Swow。

[![Contributors](https://opencollective.com/swow/contributors.svg?width=890&button=false)](https://github.com/swow/swow/graphs/contributors)

## 💬 開發 & 討論

- **文件（簡體中文）**：[https://docs.toast.run/swow/chs](https://docs.toast.run/swow/chs)
- **部落格（簡體中文）**：[https://docs.toast.run/swow-blog/chs](https://docs.toast.run/swow-blog/chs)
- **能力項（簡體中文）**：[https://docs.toast.run/swow-blog/chs/init.html](https://docs.toast.run/swow-blog/chs/init.html)
- **API 參考**：[https://docs.toast.run/swow-api/develop/](https://docs.toast.run/swow-api/develop/)
- **討論區**：[https://github.com/swow/swow/discussions](https://github.com/swow/swow/discussions)
- **計畫列表**：[https://github.com/swow/swow/projects](https://github.com/swow/swow/projects)

## 📃 開源授權

Swow 使用 [Apache License Version 2.0](http://www.apache.org/licenses/LICENSE-2.0.html) 開源。

Swow 也包含了一些第三方函式庫，這些函式庫的開源授權各不相同，請參考 [LICENSES.full](LICENSES.full) 取得完整的授權條款文字。


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
