# [v1.2.0](https://github.com/swow/swow/releases/tag/v1.2.0)

> release-date: 2023-02-26

> 成功的秘诀是开始行动。
>
> The secret of getting ahead is getting started.

## 🐣 What's New

- New API: Socket->sendFile() ([`da2397f`](https://github.com/swow/swow/commit/da2397fba8ae6d8bd7bfd17f6c9bb4d1c8650932))
- New APIs: `Coroutine->getStartTime()`, `Coroutine->getEndTime()` ([`64f516e`](https://github.com/swow/swow/commit/64f516e06515898134a161ce7ff29552884eab52))
- New Constant: WebSocket::DEFAULT_MASKING_KEY ([`72f1920`](https://github.com/swow/swow/commit/72f1920175672a0a652397ac5dff155afd8e3166))
- New Config: Introduce autoUnmask config for specific requirements ([`56088cc`](https://github.com/swow/swow/commit/56088cc8b961a12dc8e161c440c96bb28cf0122a))
- Support use PHP ini to configure async stdio/file hook and async threads (swow.async_threads=4, swow.async_file=1, swow.async_stdio=1 by default) ([`0d94d1a`](https://github.com/swow/swow/commit/0d94d1a09e568c7f100978844016ce38b263f202))

## ✨ What's Enhanced

- Support get php binary files via php-config ([#158](https://github.com/swow/swow/pull/158)) ([@sy-records](https://github.com/sy-records))
- Run update docs ci under swow organization only ([#159](https://github.com/swow/swow/pull/159)) ([@sy-records](https://github.com/sy-records))
- Handle empty maskingKey and empty data for WebSocket::unmask() ([`3d07a94`](https://github.com/swow/swow/commit/3d07a94a1bb07296a4763ce8dd1747da9ee3f576))
- Skip NULL values when pack headers ([`977c54c`](https://github.com/swow/swow/commit/977c54cf848e7445c048aacd5882e783b0a105f0))
- Add HTTP event-stream case for mixed server example ([`923e69e`](https://github.com/swow/swow/commit/923e69ef38f770606f4f1212ebc5ed02465b1852))

## 🐛 What's Fixed

- Fix stream_select() hangs on server for all platforms ([#162](https://github.com/swow/swow/issues/162))
- Fix missing SSL module init ([`90ea96c`](https://github.com/swow/swow/commit/90ea96c4ef7bd7365d0a18e3451bb569c75bf9df))
- Fix test-extension.php can not run without composer ([`329d49d`](https://github.com/swow/swow/commit/329d49d3516a0f2178e5b611d77c4cb1e117a1c0))
- Fix callable memory leak when coroutine allocation failed ([`25be041`](https://github.com/swow/swow/commit/25be04199c52844168ebf51f473b5edd831d3337))
- Some fixes for Debugger: Fix `p` and `vars` does not work; Fix missing `step_in` command ([`63a9c84`](https://github.com/swow/swow/commit/63a9c84989b33a658c64e5c1b1fc3897ee8fed44))
- Fix PHP rename() did not work for cross devices ([`9903049`](https://github.com/swow/swow/commit/9903049f068f4646ba5e1e8584878544870c68f8))
- Fix WebSocket payloadData buffer size ([`ee8bc7c`](https://github.com/swow/swow/commit/ee8bc7c8827758b02f8c8a2e54033debb091904f))
- Fix Message->addHeader() and add a test for it ([`2e51848`](https://github.com/swow/swow/commit/2e51848601ebb5e5f77055d9a1e1b49e6d2d9acc))

## 👻 What's Removed

- Remove API: Socket->setTcpAcceptBalance() ([`72e7c44`](https://github.com/swow/swow/commit/72e7c44a5f3179b2fffd39ccd5c7a2dd9e4ba719))

---

# [v1.1.0](https://github.com/swow/swow/releases/tag/v1.1.0)

> release-date: 2023-01-18

> 永远不要熄灭心中的火，哪怕别人只看见烟。
> Never extinguish the fire in your heart, even if others can only see smoke.

## 🐣 What's New

- New [README](https://github.com/swow/swow/blob/b121caa2530cb950f4d2f05e98ede9d63b18c135/README.md) ([`b121caa`](https://github.com/swow/swow/commit/b121caa2530cb950f4d2f05e98ede9d63b18c135))
- New PHP API: `Coroutine->getSwitches()`, `Coroutine::getGlobalSwitches()` ([`2ef2ac3`](https://github.com/swow/swow/commit/2ef2ac3752b6dbdf5e3bb35b78bf1eb9c9c1d621))
- Add `HttpParser` errno ([`3365aaa`](https://github.com/swow/swow/commit/3365aaa6da8e0e7c190fe74f3caae8a9245dc134)) ([`0a8c96b`](https://github.com/swow/swow/commit/0a8c96b4b8c0d9a2f15b11b2cff43a00d2b3c99e))
- New C APIs: version getters ([`f0ce087`](https://github.com/swow/swow/commit/f0ce087517127d8a73dc0e14e5f8d70eefe4cce0))

## ✨ What's Enhanced

- Do not always close all connections when closing Server (graceful shutdown)] ([`dd00372`](https://github.com/swow/swow/commit/dd003727dd5e504d78388f00911435b3e743e9b5))
- Optimizations for swow-builder ([`2ed5f60`](https://github.com/swow/swow/commit/2ed5f6058d2790256630bc585d271491248de9e7))

## 🐛 What's Fixed

- Fix bad arg name of Signal methods ([`6041422`](https://github.com/swow/swow/commit/6041422d8b816f3009a252fa1425db24acb4f2dc))

## 🐞 What's Fixed in Dependencies

- Fix va_args memory error ([libcat/libcat@`6d08909`](https://github.com/libcat/libcat/commit/6d08909b7fe48d07dabeaa5fde148a32473262b3))
- Fix bad usage of buffer value ([libcat/libcat@`6c20f9e`](https://github.com/libcat/libcat/commit/6c20f9e87f9167dabf7fe9488250f4b51fa21006))
- Fix curl easy handle exec dead loop with SSL connection ([libcat/libcat@`6f5865d`](https://github.com/libcat/libcat/commit/6f5865de631d4c7aa5d5a93ff81c26270941b04c))
- Full cURL workaround solution for SSL connection ([libcat/libcat@`1e8d176`](https://github.com/libcat/libcat/commit/1e8d1764ec30b9655b2eb52027a26e737d3ca96a))

## 👻 What's Removed

- Remove `Coroutine->getRound()`, `Coroutine::getCurrentRound()` ([`2ef2ac3`](https://github.com/swow/swow/commit/2ef2ac3752b6dbdf5e3bb35b78bf1eb9c9c1d621))

---

# [v1.0.0](https://github.com/swow/swow/releases/tag/v1.0.0)

> release-date: 2023-01-03

**Swow 是一个专注于并发 I/O 的跨平台协程引擎**，它致力于使用最小 C 核心及多数 PHP 代码以支持 PHP 高性能网络编程，具有极佳的扩展性与强大的调试能力，最大化开发者的编程效率。

Swow is a high-performance pure coroutine network communication engine, which is based on PHP and C.
It is committed to using the smallest C core and most of the PHP code to support PHP high-performance network programming.
Furthermore, it has excellent scalability and powerful debugging capabilities to maximize the programming efficiency of developers.

[Blog Post about v1.0.0](https://docs.toast.run/swow-blog/chs/init.html)
