English | [中文](./README.md)

# Swow

[![license][license-badge]][license-link]
[![ci][ci-badge]][ci-link]
[![codecov][codecov-badge]][codecov-link]
[![release][release-badge]][release-link]

> ⚠️ Preview edition, not available for real-world production. Be care.

**Swow is a high-performance pure coroutine network communication engine, which based on PHP and C**。

It is committed to using the smallest C core and most of the PHP code to support PHP high-performance network programming.

## 🚀 Coroutine

Swow implemented the most complete PHP coroutine model in history, which fully releases the power of PHP, allowing developers to do things that were unimaginable in the past.

### ⚡️High Performance⚡️

Support millions of context switches in every single second. Since Swow supports hybrid execution of coroutines of C and PHP, context switches happen only in the C stack. Plus, the Swow event scheduler is based on pure C coroutine, so that most of the coroutine switches are single-stack switches, which means the switching speed is far faster than the C and PHP dual-stack switching.

### 💫High controllability💫

Now, a coroutine can make the PHP virtual machine works like a mini operating system. Coroutines, which are executing in it, are more like processes and threads in the operating system. Developers can manipulate these coroutines within ultra-fine granularity ways. For instance, viewing the running status of all the coroutines, attaching into the coroutine, single-step debugging and tracking, viewing the coroutine stack and even each stack frame, viewing or modifying the variables in the coroutine, interrupting or killing the coroutine, etc.

Based on this controllability, developers can also use the WatchDog component to alert, interrupt, suspend or even kill the coroutines that are trapped in an infinite loop or blocked by IO, so as to prevent individual coroutines from affecting the overall stability of the program.

Moreover, its compatibility with the process design philosophy also determines the stability of the coroutine application. Just like the crash of the process will not lead to the crash of the system, the crash of the coroutine will not lead to the crash of the process. And due to PHP's powerful exception mechanism and resource management capabilities, related resources bound to the coroutine can be safely released. You don't need to pay much attention to catch Throwable and to those uncatched exceptions. Just let it crash when an unknown exception happens.

### 🌟Compatibility🌟

It is unnecessary to create coroutine and to judge whether in a coroutine environment or not. Coroutine is everywhere in Swow. Go coding by simply opening your editor! This also means that Swow will be better compatible with the existing ecosystem, and no longer need to rewrite an entry wrapped by a coroutine for PHPUnit.

## ⚙️ Event-Driven

Swow is based on coroutine event library [**libcat**](https://github.com/libcat/libcat). And the libcat is based on asynchronous event library [**libuv**](https://github.com/libuv/libuv). Thanks to libuv, Swow has an industry-level proven event loop driver, and it supports almost all mainstream operating systems. So that Swow is also the first PHP coroutine network programming engine driven by IOCP and can run on native Windows. Since libuv follows the Proactor model, Swow will soon be able to get a considerable performance improvement brought by the new feature io_uring under Linux for free.

## 🐘 Programming in PHP

The smallest C core means that the bottom layer no longer takes care of all the affairs, but only provides the most fine-grained basic interface, that is, the extension layer is no longer like a framework, but more like a library, which will make PHP maximum programming ability.

And with the PHP8 and JIT, why do we have to use C or C++ extensively to complete the work that PHP can do? Using more PHP instead of C and C++ also fits the future development direction of the PHP kernel.

Stronger programmability also brings more possibilities. Imagine that once you wanted to write a simple Waf program to implement some authentication operations by detecting IP or parsing HTTP headers, but when you get the request object in the callback, the basic instruction layers has already completed the reception for you, the complex HTTP packet parsing may have already harmed the performance of your program.

But now, the Buffer module provided by Swow allows PHP to perform memory management as finely as C. Combined with the Socket module and some protocol parsers, it allows you to master the reception and parsing of every byte. Perhaps in the future, developers can completely use Swow to write high-performance gateway programs, everything can be changed by PHP programming, and they are all memory safe.

## ✈️ Modernization

### 🐘Object-oriented🐘

Swow's growth in object-oriented is exactly the same as PHP's. Whether the early PHP or other extensions with a long history, the design of APIs is mostly process-oriented. After years of development, the PHP community has long been object-oriented. PHP also makes unremitting efforts for the object-oriented support of the built-in API. Object-oriented allows us to better perform secondary encapsulation based on the Swow library. We can directly inherit the internal classes and implement the PSR interface to support the PSR specification at zero cost, which greatly improves the performance of our programs in practical applications.

### 🛡Embracing Exception🛡

Swow is also consistent with PHP's reform philosophy in error handling. In PHP8, a large amount of notice, warning, error is eliminated, Exception-based mechanism Error/Exception is used, which greatly enhances the robustness of the program (Not let the error keep running).

Once we asked developers to check the return value after each IO operation, otherwise the program may fall into an unexpected error state. This coding style is tantamount to a historical regression. Eliminating `if ($err != null)` and embracing the exception mechanism will be the correct path of development.

Based on the above improvements, we can now write code in a chained call, making the code more concise and full of rhythm.

## 🍀 Free enhancement

Swow and Opcache are very similar. Both allow developers to make the application get free and large capacity improvements by opening extensions without changing the code. That is to ensure that the same code has consistent output results, but the instructions or system calls they run at the bottom may be different.

Therefore, Swow naturally supports all SAPIs like CLI and FPM, etc.. However, it should be noted that subject to the FPM model, you cannot simply enable Swow to directly improve performance, but it can still enhance FPM functionally, such as concurrent request interface, execution of asynchronous tasks, and so on.

In addition, even if it is a traditional synchronous blocking application, you can also use application components under the Swow ecosystem, such as using the Debugger tool provided by Swow to debug and analyze the program with breakpoints.

## 💫 Thread-safe

Swow supports running under the ZTS (Zend Thread Safety) version of PHP, that is, multi-thread support based on memory isolation.

This means that it can be used well with [parallel](https://www.php.net/manual/book.parallel.php), [pthreads](https://www.php.net/manual/book.pthreads.php) and other multi-threaded extensions. But Swow itself does not provide any multi-threading support.

## 🐣 Learning cost

Is Swow brand new? Absolutely, NO. If you are familiar with a coroutine library such as Swoole, then you can get started with Swow almost without any learning costs. On the contrary, Swow may be easier to use because it is more modern, object-oriented, embracing exception, pure coroutine, zero asynchronous callbacks, etc. All features are just to make your coding more leisurely and elegant.

In addition, if your project uses the popular community coroutine framework, its upgrade cost may be equivalent to upgrading from PHP5 to PHP7, and you may also get a 20% free performance improvement and considerable memory footprint reduction.

Swow at this stage is very suitable pioneers and geeks who want to be the first person to taste this apple. Swow will go at the forefront of PHP asynchronous coroutine technology, embrace changes and even lead changes.

## ⛓ Programming philosophy

### Two main ideas

Swow respects the CSP concurrency model, not the Callback model. In terms of implementation, a coroutine is a superset of asynchronous callbacks, and asynchronous callbacks can be simulated by creating a new coroutine, but not vice versa.

Swoole/Swow's technical choice for concurrent network programming support ends in overcoming two huge real-world technical problems: one is callback hell, and the other is the ecosystem.

### One choice

Swoole, the founder of PHP asynchronous network programming technology, tried the asynchronous callback model in the early days, however, in real-world applications, this technology often produces codes that are difficult to maintain. Nevertheless, the coroutine technology can solve the problem of poor maintainability of nested asynchronous callback codes and reduce the mental burden of developers.

The choice of stacking coroutine technology instead of the stackless coroutine is to better reuse the existing huge PHP ecosystem. At this point, other known asynchronous event libraries are moving in a direction that is more detached from the original ecosystem of PHP. In Swow, we can perfectly reuse a large number of PHP network facility libraries and develop massive PHP packages based on them without modifying any code.

### Coroutine cogitation

In addition, in the programming concept of pure coroutines, we should no longer think about code implementation in the form of asynchronous callbacks. Take the implementation of a limited number of timing tasks as an example. We should no longer think of asynchronous timers first. To use asynchronous timers, we need to save the state and count it with the help of global variable context, delete the timer after the specified number of executions. In the coroutine way, we simply create a new coroutine and specify the number of for loops, each loop suspends the specified time through sleep(), and then executes the task.

Obviously, the advantage of the coroutine approach is that the code is intuitive and the context information will not be lost. Each execution of an asynchronous timer requires a new callback stack, and in the implementation of asynchronous hybrid coroutines, a new coroutine must be created for each callback. So far, the various benefits of pure coroutine implementation are self-evident.

## ⏳ Installation

> Like any open source project, Swow always provides the strongest stability and features in the **newest release**, please try to ensure that you are using the latest version

### Compilation requirements

  - Mainstream operating systems such as Linux, Windows, macOS, Android, etc.. Same as [libuv](https://github.com/libuv/libuv/blob/v1.x/SUPPORTED_PLATFORMS.md).
  - PHP 7.3.0 or above, PHP 8.0 is recommended (The higher the version is, the better the performance you will get)

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

When installing in this way, the last step of the program will prompt for the password to install to the system with root privileges. You can choose not to enter it, and the installer will still provide a running command similar to the following:

`/usr/bin/env php -n -d extension=/path/to/vendor/swow/swow/ext/modules/swow.so --ri swow`

That is, the *.so file is output to the vendor directory of your project, which means that the extension also has version control. You can specify different versions of Swow for each of your projects without using the same *.so globally.

#### Manually install (PHP library installation is not included)

clone the Swow (You can also import it through Composer, and then cd to `vendor/swow/swow/ext` and install manually)
```shell
git clone https://github.com/swow/swow.git
````
Well known building processing. Install to the system with root privileges.
```shell
cd swow/ext && \
phpize && ./configure && make && \
sudo make install
```

#### On Windows

There is no documentation for manual build, please wait for the official binary release.

## ℹ️ IDE helper

Due to Swow's perfect strong type declaration and PHP8's complete support for built-in function and method declaration information, you can get excellent IDE prompt support for built-in classes, functions, and methods in your project by only installing Swow through Composer. And programming experience is far beyond the past.

You can read the declaration file of [built-in classes, functions, methods](lib/src/Swow.php) by yourself. It is **automatically generated** by the reflection tool, but its quality is far superior to other similar extensions through automated generation methods. You can even use it to see the default values of the parameters of the built-in functions and methods in the IDE.

Of course, more detailed API documentation will continue to be improved in the documentation.

## 📔 Sample code

You can read the example files in the [examples](examples) directory of this project. We will continue to add more examples, pursuing to achieve the goal of code as documentation and examples as tutorials.

Based on the powerful features of Swow, we can write a high-performance, strong enough, and heartbeat functioned [TCP echo server](examples/tcp_server/heartbeat.php) with just 24 lines of basic code, you can run sample code and experience by using telnet to connect!

Or use no more than one hundred lines of code to write a [HTTP+WebSocket mixed server] (examples/http_server/mixed.php) with a 100K-level QPS. The whole example is purely synchronous, zero asynchronous callbacks. It is easy to catch exceptions and various error conditions were handled.

Swow's [PHP source code](lib/src/Swow) is also a good example. Swow is an incredible tool for PHP language and network programming enthusiasts. The underlying implementation code that was once invisible can now be implemented in PHP code and displayed in front of developers. You can modify them to create a customized HTTP server that is exclusive to you.

## 🛠 Development & Discussion

  - **Chinese Document**：[https://wiki.s-wow.com](https://wiki.s-wow.com)（Not yet completed, so stay tuned）
  - **TODO**：https://github.com/swow/swow/projects

[license-badge]: https://img.shields.io/badge/license-apache2-blue.svg
[license-link]: LICENSE
[ci-badge]: https://github.com/swow/swow/workflows/swow/badge.svg
[ci-link]: https://github.com/swow/swow/actions?query=workflow:swow
[codecov-badge]: https://codecov.io/gh/swow/swow/branch/develop/graph/badge.svg?token=
[codecov-link]: https://codecov.io/gh/swow/swow
[release-badge]: https://img.shields.io/github/release/swow/swow.svg?style=flat-square
[release-link]: https://github.com/swow/swow/releases

