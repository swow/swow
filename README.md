English | [中文](./README-CN.md) | [日本語](./README-JP.md)

<h2 align="center">
<a href="https://github.com/swow/swow"><img width="320" height="86" alt="Swow Logo" src="https://docs.toast.run/assets/images/swow.svg" /></a><br />
🚀 Swow is a multi-platform support and coroutine-based engine with a focus on concurrent I/O
</h2>

[![license][license-badge]][license-link]
[![ci][ci-badge]][ci-link]
[![codecov][codecov-badge]][codecov-link]
[![release][release-badge]][release-link]
![❤️][made-with-love-badge]
![php][supported-php-versions-badge]
![platform][supported-platforms-badge]
![architecture][supported-architectures-badge]

## 👾 Design Philosophy

Swow is committed to using the smallest C core and most of the PHP code to support PHP high-performance network programming,
which determines that it is more about providing powerful secondary development capabilities by seamlessly integrating PHP code and C kernel,
while ensuring critical performance.

In addition, it provides a variety of debugging mechanisms and powerful and low-threshold debugging tools,
which can ensure that developers are free from the trouble of BUGs as much as possible,
and ordinary developers can also have the ability to debug near the level of experts with the help of tools,
so as to maximize development efficiency.

## 🎮 Installation

> Like any open source project, Swow always provides the strongest stability and features in **the newest release**, please try to ensure that you are using the latest version.

### 🔎 Requirements

- Common operating systems such as Linux, Windows, macOS, etc. Almost same as [libuv](https://github.com/libuv/libuv/blob/v1.x/SUPPORTED_PLATFORMS.md)
- PHP 8.0.0 or above, the latest version is recommended

### 👨‍🎤 By Composer

pull source code:

```shell
composer require swow/swow
```

Then run the automatic installer `swow-builder` under the `vendor/bin` directory to start the installation:

```shell
./vendor/bin/swow-builder --install
```

After you update the source code of Swow, you should use the `rebuild` option to recompile Swow and then install it:

```shell
./vendor/bin/swow-builder --rebuild --install
```

More information about `swow-builder` can be found in [Extension installation document - By Composer](https://docs.toast.run/swow/en/install.html).

### 🐧 Manual installation (UNIX-like)

clone the Swow (You can also import it through Composer, and then cd to `vendor/swow/swow/ext` and install manually):

```shell
git clone https://github.com/swow/swow.git
````

Well-known building procedure. Install to the system with root privileges:

```shell
cd swow/ext && \
phpize && ./configure && make && \
sudo make install
```

### Ⓜ️ Manual installation (Windows)

See [Installation document - Manual compilation installation (Windows)](https://docs.toast.run/swow/en/install.html#manually-build-and-install-windows).

You can also download DLL directly in [Latest Release](https://github.com/swow/swow/releases/latest).

## 🔰️ Security issues

Security issues should be reported privately, via email, to [twosee@php.net](mailto:twosee@php.net).
You should receive a response within 24 hours.
If for some reason you do not, please follow up via email to ensure we received your original message.

## 🖊️ Contribution

Your contribution to Swow development is very welcome!

You may contribute in the following ways:

* [Report issues and feedback](https://github.com/swow/swow/issues)
* [Submit fixes, features via Pull Request](https://github.com/swow/swow/pulls)
* [Write/polish documentation via GitHub](https://github.com/toastrun/docs.toast.run)

## ❤️ Contributors

PHP high-performance coroutine network communication engine development road
is too high to be popular, It's a lonely open source work at the top.

Thank you very much to the following partners for the contribution of the Swow project,
without you there is no Swow now.

[![Contributors](https://opencollective.com/swow/contributors.svg?width=890&button=false)](https://github.com/swow/swow/graphs/contributors)

## 💬 Development & Discussion

- **Wiki**：[https://docs.toast.run/swow/en](https://docs.toast.run/swow/en)
- **Blog (CHS)** [https://docs.toast.run/swow-blog/chs](https://docs.toast.run/swow-blog/chs)
- **Features (CHS)** [https://docs.toast.run/swow-blog/chs/init.html](https://docs.toast.run/swow-blog/chs/init.html)
- **API Reference**：[https://docs.toast.run/swow-api/ci.html](https://docs.toast.run/swow-api/ci.html)
- **Discussions**：[https://github.com/swow/swow/discussions](https://github.com/swow/swow/discussions)
- **TODO**：[https://github.com/swow/swow/projects](https://github.com/swow/swow/projects)

## 📃 License

Swow is available under the [Apache License Version 2.0](http://www.apache.org/licenses/LICENSE-2.0.html).
Swow also includes external libraries that are available under a variety of licenses.
See [LICENSES.full](LICENSES.full) for the full license text.

[license-badge]: https://img.shields.io/badge/license-apache2-blue.svg
[license-link]: LICENSE
[ci-badge]: https://github.com/swow/swow/workflows/tests/badge.svg
[ci-link]: https://github.com/swow/swow/actions?query=workflow:tests
[codecov-badge]: https://codecov.io/gh/swow/swow/branch/develop/graph/badge.svg
[codecov-link]: https://codecov.io/gh/swow/swow
[release-badge]: https://img.shields.io/github/release/swow/swow.svg?include_prereleases
[release-link]: https://github.com/swow/swow/releases
[made-with-love-badge]: https://img.shields.io/badge/made%20with-%E2%9D%A4-f00
[supported-php-versions-badge]: https://img.shields.io/badge/php-8.0--8.3-royalblue.svg
[supported-platforms-badge]: https://img.shields.io/badge/platform-Win32%20|%20GNU/Linux%20|%20macOS%20|%20FreeBSD%20-gold
[supported-architectures-badge]: https://img.shields.io/badge/architecture-x86--64%20|%20ARM64%20|%20mips64el%20|%20riscv64%20-maroon
