# Swow

English | [中文](./README-CN.md)

[![license][license-badge]][license-link]
[![ci][ci-badge]][ci-link]
[![codecov][codecov-badge]][codecov-link]
[![release][release-badge]][release-link]
![❤️][made-with-love-badge]
![php][supported-php-versions-badge]
![platform][supported-platforms-badge]
![architecture][supported-architectures-badge]

> ⚠️ Preview version, not yet production ready. Use it with care.

**Swow is a high-performance pure coroutine network communication engine, which based on PHP and C.**

It is committed to using the smallest C core and most of the PHP code to support PHP high-performance network programming.

## 🚀 Coroutine

Swow implemented the most complete PHP coroutine model so far, which fully releases the power of PHP, allowing developers to do things that were unimaginable in the past.

### ⚡️High Performance⚡️

Support millions of context switches in every single second. Since Swow supports hybrid execution of coroutines of C and PHP, context switches happen only in the C stack. Plus, Swow event scheduler is based on pure C coroutine, so that most of the coroutine switches are single-stack switches, which means the switching speed is far faster than the C and PHP dual-stack switching.

### 💫High controllability💫

Now, coroutine can make the PHP virtual machine works like real operating system. Coroutines, which was executing in PHP virtual machine, are more like processes and threads in operating system. Developers can manipulate these coroutines within ultra-fine granularity ways. For instance, inspecting the running status of all the coroutines, attaching into one of them, single-step debugging and tracking, dumping the coroutine stack and even each stack frame, inspecting or modifying the variables in coroutines, interrupting or killing the coroutine, etc.

Benefit from this controllability, developers can also use WatchDog component to notify, interrupt, suspend or even kill the coroutines that are trapped in an infinite loop or a blocking IO operation, so as to prevent a particular coroutine from affecting overall stability of a program.

Moreover, process design pattern also makes coroutine application robust. Just like the crash of a single process will not cause the entire operating system to crash, the crash of a single coroutine will not cause the entire process to crash. And thanks to PHP's powerful exception mechanism and resource management capabilities, related resources bound to the crashed coroutine can be safely released. You needn't to take heavy effort to catch all kinds of exceptions and deal with uncatched exceptions. Just let the coroutine crash when an unknown exception happens.

### 🌟Compatibility🌟

Forget about platform gaps, coroutine is just run every platform in Swow! Go coding by simply opening your editor! This also means that Swow have good compatibility with existing ecosystem: We no longer need to write a coroutine-wrapped entrypoint for PHPUnit!

## ⚙️ Event-Driven

Swow is based on coroutine event library [**libcat**](https://github.com/libcat/libcat). And libcat is based on asynchronous event library [**libuv**](https://github.com/libuv/libuv). Thanks to libuv, Swow has an industry-level proven event loop driver, and it supports almost all common operating systems. So that Swow is also the first PHP coroutine network programming engine driven by IOCP and can run on Windows natively. Since libuv follows the Proactor model, Swow will soon be able to get a considerable performance improvement brought by the new feature io_uring under Linux for free.

## 🐘 Programming in PHP

The smallest C core means that the bottom layer no longer takes care of all the affairs, but only provides the most fine-grained basic interface, that is, the extension layer is no longer like a framework, but more like a library, which will maximize PHP programming capability.

Why do we have to use C or C++ extensively to complete works that PHP can do with PHP8 and JIT? Using more PHP instead of C and C++ also fits the future of PHP development.

Stronger programmability also brings more possibilities. Imagine that once you wanted to write a simple WAF program to implement some authentication operations by detecting IP or parsing HTTP headers, but when you get the request object in the callback, the basic instruction layers has already completed the reception for you, the complex HTTP packet parsing may have already harmed the performance of your program.

But now, the buffer module provided by Swow allows PHP to perform memory management as fine as C. Combined with the Socket module and some protocol parsers, it allows you to master the reception and parsing of every byte. Perhaps in the future, developers can use Swow only to implement high-performance gateway, everything can be changed by programming in PHP, and they are all memory safe.

## ✈️ Modernization

### 🐘Object-oriented🐘

Swow's growth in object-oriented is exactly the same as PHP's. Neither early PHP nor some extensions with long history have object-oriented APIs. After years of development, PHP community has long been object-oriented. PHP also makes unremitting efforts for the object-oriented support of built-in APIs. Object-oriented allows us to better perform secondary encapsulation based on the Swow library. We can directly inherit the internal classes and implement the PSR interface to support the PSR specification at no cost, which greatly improves the performance of our programs in practical application sences.

### 🛡Embracing Exception🛡

Swow is also consistent with PHP's new error handling policy. In PHP8, a large amount of notice, warning, error is eliminated, Exception-based error handling mechanism is introduced, which greatly enhances the robustness of the program (Do not keep a program running after an error occurred).

Once we asked developers to check the return value after each IO operation, otherwise the program may fall into an unexpected error state. This coding style is tantamount to a historical regression. Eliminating `if ($err != null)` and embracing the exception mechanism will be the correct path of development.

Based on the above improvements, we can now write code in method chaining style, making the code more concise and full of rhythm.

## 🍀 Free enhancement

Like Opcache, Swow allow developers to make an application get free impressive improvements by opening extensions without changing its code. That is to ensure that the same code has consistent behaviors, but the CPU instructions or system calls they based on may be different.

Therefore, Swow naturally supports all SAPIs like CLI and FPM, etc.. However, it should be noted that subject to the FPM model, you cannot improve performance instantly by only enabling Swow in FPM, but it can still enhance FPM capability, such as concurrent remote interfaces requests, execution of asynchronous tasks, and so on.

In addition, even if it is a traditional synchronous blocking application, you can also use application components under the Swow ecosystem, such as using Swow Debugger tool to debug and analyze the program with breakpoints.

## 💫 Thread-safe

Swow supports running under the ZTS (Zend Thread Safety) version of PHP, that is, multi-thread support based on memory isolation.

This means that it can be used well with [parallel](https://www.php.net/manual/book.parallel.php), [pthreads](https://www.php.net/manual/book.pthreads.php) and other multi-threaded extensions. But Swow itself does not provide any multi-threading support.

## 🐣 Learning cost

Is Swow brand new? Absolutely, NO. If you are familiar with a coroutine library such as Swoole, then you can get started with Swow almost without any extra learning costs. On the contrary, Swow may be easier to learn because it is more modern, object-oriented, exception embracing, pure coroutine, no asynchronous callbacks, etc. All features are just to make coding more enjoyable and elegant.

In addition, if your project uses popular community coroutine frameworks, its upgrading experience may be like upgrading from PHP5 to PHP7, and you may also get a 20% free performance improvement and considerable memory footprint reduction.

Swow at this stage is very suitable for pioneers and geeks who want to be the first person to taste this apple. Swow will go at the forefront of PHP asynchronous coroutine technology, embrace changes and even lead changes.

## ⛓ Programming philosophy

Swow follows the CSP concurrency model, not the Callback model. A coroutine can be a superset of asynchronous callbacks, therefore asynchronous callbacks can be simulated by creating a new coroutine, but not vice versa.

### Two main ideas

Swoole/Swow's technical choice for concurrent network programming model ends in overcoming two huge practical difficulty: one is callback hell, and the other is ecosystem adoption.

### One choice

Swoole, the founder of PHP asynchronous network programming model, tried the asynchronous callback model in its early developments, however, in real-world applications, this model often needs codes which are difficult to maintain. Nevertheless, the coroutine model can get rid of nested asynchronous callback codes to reduce developers' additional works.

Choosing stacking coroutine model instead of stackless one is better for reusing the existing huge PHP ecosystem. At this point, other known asynchronous event libraries are trend to detaching from the original ecosystem of PHP. In Swow, we can reuse a large number of PHP network facilities perfectly and develop massive PHP packages based on these facilities without modifying any code.

### Understanding Coroutine

In pure coroutine model, we should just forget asynchronous callbacks. For example, to implement a finite periodic event trigger with interval: To use asynchronous timers, we need to save the state in global variable context to count how many rounds we have ran, at last delete the timer after certain number of rounds. But in coroutine way, we simply create a new coroutine and simply start a loop for certain rounds in it, at each loop suspends the specified time through sleep(), and then executes this coroutine.

Obviously, the advantage of the coroutine approach is that the code is intuitive and the context information will not be lost. Each execution of an asynchronous timer requires a new callback stack, and in the implementation of asynchronous hybrid coroutines, a new coroutine must be created for each callback. So far, the various benefits of pure coroutine implementation are self-evident.

## ⏳ Installation

> Like any open source project, Swow always provides the strongest stability and features in the **newest release**, please try to ensure that you are using the latest version

### Compilation requirements

- Common operating systems such as Linux, Windows, macOS, etc.. Same as [libuv](https://github.com/libuv/libuv/blob/v1.x/SUPPORTED_PLATFORMS.md).
- PHP 8.0.0 or above, the latest version is recommended (The higher the version is, the better the performance you will get)

#### By Composer

```shell
composer require swow/swow:dev-develop
```

Start the installation by executing the `swow-builder` under `vendor/bin`

| Command cases                                       | Function                          |
| --------------------------------------------------- | --------------------------------- |
| ./vendor/bin/swow-builder                           | Ordinary build                    |
| ./vendor/bin/swow-builder --rebuild                 | Rebuild (clean build)             |
| ./vendor/bin/swow-builder --show-log                | Show log during build             |
| ./vendor/bin/swow-builder --debug                   | Build with debug                  |
| ./vendor/bin/swow-builder --enable="--enable-debug" | Specify special build parameters  |

When installing in this way, the last step of the program will try use `sudo` to install the extension without futher asking, and `sudo` may ask you for password to install Swow into your system path.

It may also prompts commands like:

`/usr/bin/env php -n -d extension=/path/to/vendor/swow/swow/ext/modules/swow.so --ri swow`

That is, the \*.so file is output to the vendor directory of your project, which means that the extension also has version control. You can specify different versions of Swow for each of your projects without using the same \*.so globally.

#### Manually install (PHP library installation is not included)

clone the Swow (You can also import it through Composer, and then cd to `vendor/swow/swow/ext` and install manually)

```shell
git clone https://github.com/swow/swow.git
````

Well known building procedure. Install to the system with root privileges.

```shell
cd swow/ext && \
phpize && ./configure && make && \
sudo make install
```

#### On Windows

There is [document(CN)](https://wiki.s-wow.com/#/zh-cn/install?id=%e7%bc%96%e8%af%91%e5%ae%89%e8%a3%85-windows) for manual build, you can also use official DLL releases.

## ℹ️ IDE helper

Due to Swow's perfect strong type declaration and PHP8's complete support for built-in function and method declaration information, you can get excellent IDE prompt support for built-in classes, functions, and methods in your project by only installing Swow through Composer. And programming experience is far beyond the past.

You can read the declaration file of [built-in classes, functions, methods](lib/src/Swow.php) by yourself. It is **automatically generated** by the reflection tool, but its quality is far superior to other similar extensions through automated generation methods. You can even use it to see the default values of the parameters of the built-in functions and methods in the IDE.

Of course, more detailed API documentation will continue to be improved in the documentation.

## 📔 Sample code

You can read the example files in the [examples](examples) directory of this project. We will continue to add more examples, pursuing to achieve the goal of code as documentation and examples as tutorials.

Based on the powerful features of Swow, we can write a high-performance, strong enough, and heartbeat functioned [TCP echo server](examples/tcp_server/heartbeat.php) with just 24 lines of basic code, you can run sample code and experience by using telnet to connect!

Or use no more than one hundred lines of code to write a [HTTP+WebSocket mixed server](examples/http_server/mixed.php) with a 100K-level QPS. The whole example is purely synchronous, zero asynchronous callbacks. It is easy to catch exceptions and various error conditions were handled.

Swow's [PHP source code](lib/src/Swow) is also a good example. Swow is an incredible tool for PHP language and network programming enthusiasts. The underlying implementation code that was once invisible can now be implemented in PHP code and displayed in front of developers. You can modify them to create a customized HTTP server that is exclusive to you.

## 🛠 Development & Discussion

- **Chinese Document**：[https://wiki.s-wow.com](https://wiki.s-wow.com)（Not yet completed, so stay tuned）
- **TODO**：[https://github.com/swow/swow/projects](https://github.com/swow/swow/projects)

[license-badge]: https://img.shields.io/badge/license-apache2-blue.svg
[license-link]: LICENSE
[ci-badge]: https://github.com/swow/swow/workflows/tests/badge.svg
[ci-link]: https://github.com/swow/swow/actions?query=workflow:tests
[codecov-badge]: https://codecov.io/gh/swow/swow/branch/develop/graph/badge.svg
[codecov-link]: https://codecov.io/gh/swow/swow
[release-badge]: https://img.shields.io/github/release/swow/swow.svg?include_prereleases
[release-link]: https://github.com/swow/swow/releases
[made-with-love-badge]: https://img.shields.io/badge/made%20with-%E2%9D%A4-f00
[supported-php-versions-badge]: https://img.shields.io/badge/php-8.0--8.2-royalblue.svg
[supported-platforms-badge]: https://img.shields.io/badge/platform-Win32%20|%20GNU/Linux%20|%20macOS%20|%20FreeBSD%20-gold
[supported-architectures-badge]: https://img.shields.io/badge/architecture-x86--64%20|%20ARM64%20|%20mips64el%20|%20riscv64%20-maroon
