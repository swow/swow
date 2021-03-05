[English](./README.md) | 中文

# Swow

[![license][license-badge]][license-link]
[![ci][ci-badge]][ci-link]
[![codecov][codecov-badge]][codecov-link]
[![release][release-badge]][release-link]

> ⚠️ 预览版本，非生产可用

**Swow是一个使用PHP和C编写的高性能纯协程网络通信引擎**。

它致力于使用最小C核心及多数PHP代码以支持PHP高性能网络编程。

## 🚀 协程

Swow实现了一套有史以来最完整的PHP协程模型，它全面释放了PHP的真正实力，使得开发者可以做到以往难以想象的事情。

### ⚡️高性能⚡️

支持每秒百万次上下文切换。且由于Swow支持纯C协程与PHP协程混合运行，两者之间仅进行C栈单栈上下文切换，且Swow的事件调度器就是纯C协程，因此大部分协程切换都是单栈切换，其切换速度远超C + PHP的双栈切换。

### 💫高可控💫

现在协程可以使得PHP虚拟机像一个迷你操作系统一样，其中运行的协程近似于操作系统的进程/线程，开发者可以以超细粒度任意操控这些协程，如查看所有协程的运行状态、attach进入协程、单步调试跟踪、查看协程堆栈乃至每个栈帧、查看或修改协程内变量、对协程进行中断或杀死协程等。

基于这样的可控性，开发者还可以借助WatchDog组件对陷入死循环或处于IO阻塞的协程进行警报、中断、让出甚至杀死等多种处理，以避免个别协程影响到程序整体稳定性。

并且，其与进程设计哲学的契合性，也决定了协程应用的健壮性极佳。正如进程的崩溃不会导致系统的崩溃一样，协程的崩溃也不会导致进程的崩溃，并且得益于PHP强大的异常机制和资源管理能力，与协程绑定的相关资源都能被安全地释放。你无需再为未捕获的异常殚精竭力，也不要再去捕获Throwable，遇到未知的错误请Let it crash。

### 🌟易兼容🌟

无需创建协程也无需判断是否在协程环境，在Swow中，所及之处，皆为协程，打开编辑器，即刻就可以开始书写你的代码！这也意味着它将更好地兼容已有的生态，我们再也不需要为PHPUnit重写一个由协程包裹的入口。

## ⚙️ 事件驱动

Swow基于协程事件库[**libcat**](https://github.com/libcat/libcat)开发，libcat又基于异步事件库[**libuv**](https://github.com/libuv/libuv)开发，得益于此，我们拥有了经过工业级验证的事件循环驱动，并且它支持几乎所有主流操作系统。因此，Swow也是首个能在原生Windows平台运行，且通过IOCP驱动的PHP协程网络编程引擎。由于libuv遵循Proactor模型，不久后我们又可以在Linux下免费获得由新特性io_uring带来的可观性能提升。

## 🐘 PHP可编程性

最小C核心意味着底层不再包揽所有事务，而是仅提供最细粒度的基础接口，即扩展层不再像是一个框架(framework)，而更像是库(library)，这将使PHP的编程能力最大化。

且随着PHP8和JIT的到来，我们为什么还要大量使用C或是C++来完成那些PHP也能做到的工作呢？更多地使用PHP而不是C、C++，也契合了PHP内核的未来发展方向。

更强的可编程性也带来了更多的可能性。试想一下，曾经你想写一个简单的Waf程序，通过检测IP或是解析HTTP头部来实施一些鉴权操作，但当你在回调中拿到请求对象时，底层已经替你完成了整个请求的接收，庞大的HTTP报文解析或许已经损害了你的程序性能。

但现在，Swow提供的Buffer模块使得PHP可以像C一样精细地进行内存管理，结合Socket模块和一些协议解析器，它允许你掌握小到每个字节的接收和解析，或许在未来开发者完全可以使用Swow编写高性能的网关程序，一切都可以通过PHP编程的方式改变，且它们全都是内存安全的。

## ✈️ 现代化

### 🐘面向对象🐘

Swow在面向对象的道路上和PHP的进化路线如出一辙，不管是早期的PHP或是其它有悠久历史的扩展，对于API的设计大都是面向过程的，而经过多年发展，PHP社区早已是面向对象的天下，PHP也为内建API的面向对象支持而不懈努力。面向对象使得我们可以更好地基于Swow库进行二次封装，我们可以直接继承内部类并实现PSR接口以0成本支持PSR规范，这在实际应用中让我们的程序性能得到了极大的提升。

### 🛡拥抱异常🛡

Swow在错误处理方面和PHP的改革理念也是一致的，PHP8干掉了大量的notice、warning、error，转而使用基于异常机制的Error/Exception，极大地增强了程序的健壮性（不要再让错误的程序继续运行下去）。

曾经我们要求开发者在每个IO操作后检查返回值，否则程序就可能陷入非预期的错误状态中，这样的编码方式，无异于是一种历史的倒退，干掉`if ($err != null)`，拥抱异常机制，才是正确的发展道路。

而基于上述改进，我们现在能以链式调用的方式书写代码，使代码变得更加简洁且富有节奏感。

## 🍀 绿色增强

Swow和Opcache非常相似，两者都允许开发者在几乎不改变代码的情况下，仅通过开启扩展就能使得应用程序获得免费大量的能力提升。即保证相同的代码具有一致的输出结果，但它们在底层所运行的指令或是系统调用可能不尽相同。

因此，Swow理所当然地支持所有SAPI（CLI，FPM等）。但需要注意的是，受制于FPM的模型，你无法简单地通过开启Swow直接获得性能上的提升，但它仍能在功能上对FPM进行增强（如并发请求接口、执行异步任务等）。

此外，即使是传统的同步阻塞应用，也可以使用Swow生态下的应用组件，如使用Swow提供的Debugger工具对程序进行断点调试分析等。

## 💫 线程安全

Swow支持在PHP的ZTS（Zend Thread Safety）版本下运行，即基于内存隔离的多线程支持。

这意味着它可以和[parallel](https://www.php.net/manual/book.parallel.php)、[pthreads](https://www.php.net/manual/book.pthreads.php)等多线程扩展很好地结合使用，但Swow本身不会提供任何多线程支持。

## 🐣 学习成本

Swow是否又是一个全新的轮子？答案是否定的。如果你熟悉如Swoole这样的协程库，那么你几乎无需任何学习成本就可以上手Swow，反之，Swow可能更易于上手，因为它更现代化，面向对象、异常机制、纯协程、零异步回调等一切特性都只为了让你的编码更加从容优雅。

此外，如果你的项目使用了社区流行协程框架，那么它的升级成本可能仅和由PHP5升级到PHP7相当，并且你还可能因此获得20%的免费性能提升和可观的内存占用缩减。

现阶段的Swow非常适合技术先驱和极客们尝鲜，它将走在PHP异步协程技术的前沿，拥抱变化甚至引领变化。

## ⛓ 编程理念

### 两个问题

Swow推崇CSP的并发模型，而不是Callback模型。在实现上来说，协程是异步回调的超集，通过创建新的协程可以模拟异步回调，而反之则不行。

Swoole/Swow对于并发网络编程支持的技术抉择终结于克服两个巨大的现实技术难题：一个是回调地狱，另一个是生态。

### 一个选择

PHP异步网络编程技术的奠基者Swoole早期就尝试了异步回调模型，但在实际开发中该技术常常制造出难以维护的代码，而协程技术可以解决嵌套异步回调代码可维护性极差的问题并减轻开发者的心智负担。

而选择有栈协程技术而不是无栈协程，则是为了更好地复用已有的庞大的PHP生态。在这一点上，其它已知的异步事件库都走在了与PHP原有生态更加割裂的方向上。在Swow中，我们可以完美复用大量的PHP网络设施库及基于它们开发的海量PHP包支持而无需修改任何代码。

### 协程思维

此外，在纯协程的编程理念中，我们不应再以异步回调的方式去思考代码实现。以实现一个有限次数的定时任务为例，我们不再应该先想到异步定时器，使用异步定时器实现我们需要借助全局变量上下文保存状态并对其计数，在执行指定次数后删除定时器，而在协程的方式下，我们只是简单地新建一个协程并for循环指定次数，每次循环通过sleep()挂起指定的时间，再执行任务。

显然协程方式的好处是代码直观且上下文信息不会丢失。异步定时器的每次执行都需要一个全新的回调堆栈，在异步混合协程的实现下，每次回调都必须创建一个新的协程，此时纯协程实现的各种好处就不言而喻了。

## ⏳ 安装

> 和任何开源项目一样, Swow总是在**最新的发行版**提供最强的稳定性和功能, 请尽量保证你使用的是最新版本

### 编译需求

  - Linux、 Windows、macOS、Android等主流操作系统（与[libuv](https://github.com/libuv/libuv/blob/v1.x/SUPPORTED_PLATFORMS.md)一致）
  - PHP 7.3.0 或以上版本，推荐使用PHP 8.0 (版本越高性能越好)

#### Composer安装

```shell
composer require swow/swow:dev-develop
```

然后运行`vendor/bin`目录下的自动安装器`swow-builder`即可开始安装

| 命令示例                                            | 用途                |
| --------------------------------------------------- | ------------------- |
| ./vendor/bin/swow-builder                           | 普通构建            |
| ./vendor/bin/swow-builder --rebuild                 | 重新构建 (清洁构建) |
| ./vendor/bin/swow-builder --show-log                | 构建时显示日志      |
| ./vendor/bin/swow-builder --debug                   | 构建debug版本       |
| ./vendor/bin/swow-builder --enable="--enable-debug" | 指定特殊的构建参数  |

使用此方式安装时，最后一步程序会提示输入密码以使用root权限安装到系统，你可以选择不输入，安装程序仍会提供类似于以下的运行命令：

`/usr/bin/env php -n -d extension=/path/to/vendor/swow/swow/ext/modules/swow.so --ri swow`

即扩展的so文件输出到了你项目的vendor目录下，相当于扩展也拥有了版本控制，你可以为你的每个项目指定不同版本的Swow，而无需全局使用同一个扩展so。

#### 手动安装扩展 (不包含PHP库部分)

克隆项目（也可以通过Composer引入，cd到`vendor/swow/swow/ext`中手动编译）
```shell
git clone https://github.com/swow/swow.git
````
熟悉的构建三板斧，最后使用root权限安装到系统
```shell
cd swow/ext && \
phpize && ./configure && make && \
sudo make install
```

#### Windows安装

手动构建暂无文档，请等待官方二进制发布，敬请期待。

## ℹ️ IDE助手

得益于Swow完善的强类型声明和PHP8对于内置函数、方法声明信息的完备支持，仅通过Composer安装Swow即可在你的项目中获得绝佳的内置类、函数、方法的IDE提示支持，编程体验远超以往。

你可以自行阅览[内置类、函数、方法](lib/src/Swow.php)的声明文件，它是由反射工具**自动生成**的，但其质量远超其他同类扩展通过自动化生成方式生成的声明质量，你甚至可以在IDE中借助其看到内置函数、方法的参数默认值。

当然，更详细的API文档会在文档中不断完善。

## 📔 示例代码

你可以在本项目的[examples](examples)目录下阅览示例文件，我们会不断新增更多示例，追求达到代码即文档、示例即教程的目标。

基于Swow的强大特性，我们可以用短短二十四行基础代码写出一个高性能、健壮性强、带心跳功能的[TCP回显服务器](examples/tcp_server/heartbeat.php)，你可以运行示例代码并用telnet连接体验！

或是用不超过一百行代码写出一个百K级QPS的[HTTP+WebSocket混合服务器](examples/http_server/mixed.php)，整个示例纯同步、零异步回调，还通过异常捕获很轻松地处理了各种错误情况。

Swow的[PHP源代码](lib/src/Swow)也是良好的示例，Swow是为PHP语言和网络编程开发爱好者打造的神兵利器，曾经不可见的底层实现代码，现在都能以PHP代码的形式展现在开发者的眼前，并且你可以自由修改它们，打造出专属于你的定制化HTTP服务器。

## 🛠 开发 & 讨论

  - **中文文档**：[https://wiki.s-wow.com](https://wiki.s-wow.com)（尚未完成，敬请期待）
  - **计划列表**：https://github.com/swow/swow/projects

[license-badge]: https://img.shields.io/badge/license-apache2-blue.svg
[license-link]: LICENSE
[ci-badge]: https://github.com/swow/swow/workflows/tests/badge.svg
[ci-link]: https://github.com/swow/swow/actions?query=workflow:tests
[codecov-badge]: https://codecov.io/gh/swow/swow/branch/develop/graph/badge.svg?token=
[codecov-link]: https://codecov.io/gh/swow/swow
[release-badge]: https://img.shields.io/github/release/swow/swow.svg?style=flat-square
[release-link]: https://github.com/swow/swow/releases

