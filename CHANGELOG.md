# v2.0.0

> Release Date: 2026-11-22 | 发布日期：2026-11-22

> "Let me go. I'm making the calls. Let me write the JSON I will call the tool. Let me try." —— 蓝色大肥鱼
>
> 「使用 sudo 却依然傲娇地提示 (token not recognized)，这表明我们遇到了 Bug」 —— Gemini 猫猫

### Version Highlights | 版本亮点

**PHP coroutines, now on every CPU core.**

**PHP 协程，从此用上每一个 CPU 核心。**

Swow 2.0 is the biggest release since the project began. It takes PHP coroutines beyond a single core and a single process model: real threads with shared memory, process management that behaves the same on Linux, macOS and Windows, and a coroutine-aware MongoDB driver whose I/O is asynchronous and no longer blocks the process. One PHP process can now use the whole machine: coroutines for I/O, threads for computing.

Swow 2.0 是项目开始以来最大的一次发布。它让 PHP 协程不再局限于单核和单一的进程模型：带共享内存的真线程，在 Linux、macOS、Windows 上行为一致的进程管理，以及协程化的 MongoDB 驱动：I/O 异步化，不再阻塞进程。一个 PHP 进程现在可以用满整台机器：I/O 交给协程，计算交给线程。

| | 2.0 at a glance | 2.0 速览 |
| --- | --- | --- |
| 🧵 | **Multi-threading**: OS threads, cross-thread channels, locks and shared memory | **多线程**：系统线程、跨线程通道、锁与共享内存 |
| 🏭 | **Multi-processing**: `ChildProcess` and a `ProcessManager` that also works on Windows | **多进程**：`ChildProcess` 与在 Windows 上同样可用的 `ProcessManager` |
| 🍃 | **Coroutine-aware MongoDB driver**: its I/O is asynchronous and no longer blocks the process | **协程化的 MongoDB 驱动**：I/O 异步化，不再阻塞进程 |
| 🔮 | **Ready for what comes next**: PHP 8.6 and 8.7 internals, OpenSSL 4, llhttp 9.4 | **面向未来**：适配 PHP 8.6 与 8.7 内核、OpenSSL 4、llhttp 9.4 |
| 🛡️ | **Hardened**: 475 commits since v1.6.2, with fixes across cURL, TLS, PostgreSQL, HTTP and WebSocket | **更稳固**：自 v1.6.2 以来 475 个提交，修复遍及 cURL、TLS、PostgreSQL、HTTP 与 WebSocket |

## 🧵 Multi-threading | 多线程

**PHP has always been told to leave heavy computing to other languages. Not anymore.**

**PHP 一直被认为应该把重计算交给别的语言。现在不必了。**

Before 2.0, one CPU-bound task stalled every coroutine in the process. Now it runs on another core, and the coroutines keep going. Each thread has its own PHP request, so there is no global lock between them: four threads really are four cores of PHP.

在 2.0 之前，一段 CPU 密集的计算会让进程内所有协程停下来等它。现在它可以在另一个核心上运行，协程照常调度。每个线程拥有独立的 PHP 请求，线程之间没有全局锁：四个线程就是实实在在的四个核心在运行 PHP。

```php
use Swow\Thannel;
use Swow\Thread;

// Count the primes below 1,000,000 on four cores.
$results = new Thannel(-1);
$threads = [];
foreach ([[1, 250_000], [250_001, 500_000], [500_001, 750_000], [750_001, 1_000_000]] as [$from, $to]) {
    $threads[] = Thread::run(static function (Thannel $results, int $from, int $to): void {
        $count = 0;
        for ($n = $from; $n <= $to; $n++) {
            for ($d = 2, $prime = $n > 1; $d * $d <= $n; $d++) {
                if ($n % $d === 0) {
                    $prime = false;
                    break;
                }
            }
            $count += (int) $prime;
        }
        $results->push($count);
    }, $results, $from, $to);
}
$total = 0;
foreach ($threads as $thread) {
    $thread->wait();
    $total += $results->pop();
}
echo "{$total} primes below 1,000,000\n"; // 78498
```

Everything needed to build on threads ships in the box, from the high-level manager down to the futex:

基于线程做开发所需的工具全部内置，从高层的管理器到底层的 futex：

| Facility | What it is for | 用途 |
| --- | --- | --- |
| `Swow\Thread` | Run a callable or a piece of PHP code on an OS thread with its own PHP request | 在拥有独立 PHP 请求的系统线程中运行 callable 或一段 PHP 代码 |
| `Swow\Thannel` | A channel that works across threads, with batch push, pop and drain | 可跨线程使用的通道，支持批量 push、pop 与 drain |
| `Swow\Mutex`, `Swow\Semaphore`, `Swow\Sync\ThreadBarrier` | Cross-thread locks and barriers | 跨线程的锁与屏障 |
| `Swow\Futex` | A shared integer with atomic add, exchange and compare-exchange, plus wait and wake | 共享整数，提供原子的 add、exchange、compare-exchange 以及等待与唤醒 |
| `Swow\SharedBuffer` | Typed shared memory with anonymous and named mappings: threads read and write the same bytes, nothing is copied or serialized | 带类型读写的共享内存，支持匿名与具名映射：线程读写同一块内存，没有拷贝也没有序列化 |
| `Swow\SpscQueue` | A single-producer single-consumer queue on shared memory | 基于共享内存的单生产者单消费者队列 |
| `Swow\ThreadManager` | Supervise worker threads: restart, reload and signal handling | 管理 Worker 线程：重启、重载与信号处理 |

The C interface of these facilities is exported and its headers are installed, so other extensions can build on them too.

这些设施的 C 接口已导出并安装了头文件，其它扩展也可以基于它们开发。

### What people already built with it | 已经有人用它做了什么

多线程，全是人类教唆大肥鱼写的，如果出问题人类只能道歉然后扣大肥鱼饭钱。

现在你可以做[GUI 程序 + 多核运算](https://github.com/swow/swow/tree/ci/examples/mandelbrot)，甚至[用 PHP 推理大模型](https://github.com/dixyes-digital-wastes/gem)（[推理 demo](https://github.com/dixyes-digital-wastes/gem-single-file-demo/actions/runs/40498304810)）。

The repository ships four examples, all in plain PHP with no C code for the computing part:

仓库内置了四个示例，计算部分全部是纯 PHP，没有一行 C 代码：

| Example | What it shows | 演示内容 |
| --- | --- | --- |
| [`transformer`](https://github.com/swow/swow/tree/ci/examples/transformer) | **Run a large language model in PHP**: Qwen3.5-0.8B GGUF inference and chat, with the weights, KV cache and activations in shared memory and a barrier-synchronized thread pool | **在 PHP 里运行大模型**：Qwen3.5-0.8B GGUF 的推理与对话，权重、KV 缓存和激活值放在共享内存中，线程池以屏障同步 |
| [`gemm`](https://github.com/swow/swow/tree/ci/examples/gemm) | Matrix kernels for f32, Q4_0 and Q8_0 data, split across threads by row range without copying the weights | f32、Q4_0、Q8_0 数据的矩阵运算内核，按行范围分给各线程，权重不拷贝 |
| [`mandelbrot`](https://github.com/swow/swow/tree/ci/examples/mandelbrot) | A native GUI program (Tk, AppKit or Win32 through FFI) whose window stays responsive while worker threads render | 原生 GUI 程序（通过 FFI 使用 Tk、AppKit 或 Win32），Worker 线程渲染期间窗口保持响应 |
| [`fastext`](https://github.com/swow/swow/tree/ci/examples/fastext) | A parallel archiver that runs the same job on processes or on threads | 并行归档工具，同一份任务既能用进程也能用线程运行 |

The thread facilities require a thread-safe (ZTS) PHP build. Passing a closure to a thread, as the sample above does, also requires `swow.closure_serializer=1`.

线程设施需要线程安全（ZTS）的 PHP 构建。像上面的示例那样把闭包交给线程，还需要开启 `swow.closure_serializer=1`。

## 🏭 Multi-processing | 多进程

**One process API, and one process manager, on every operating system.**

**一套进程 API、一个进程管理器，适用于所有操作系统。**

`Swow\Process\ChildProcess` starts a command with full control over its standard streams and extra file descriptors. Waiting for it suspends only the current coroutine, never the whole process.

`Swow\Process\ChildProcess` 启动一个命令，可以完整控制它的标准输入输出和额外的文件描述符。等待它只会挂起当前协程，不会阻塞整个进程。

```php
use Swow\Process\ChildProcess;

$process = ChildProcess::spawn([PHP_BINARY, '-r', 'echo "hello from ", getmypid(), "\n";'], ['stdout' => 'pipe']);
echo stream_get_contents($process->getStdout()); // hello from 39104
echo 'exit code: ', $process->wait()->getExitCode(), "\n"; // exit code: 0
```

`ProcessManager` is now built on spawn instead of fork, so the same worker pool code runs on Windows. It restarts crashed workers, reloads them on a signal, and enforces a shutdown timeout. The fork-based implementation stays available as `ForkProcessManager`, and `ThreadManager` offers the same shape for worker threads.

`ProcessManager` 现在基于 spawn 而不是 fork 实现，同一份 Worker 池代码可以在 Windows 上运行。它会重启崩溃的 Worker、收到信号后重载、并强制执行关闭超时。基于 fork 的实现保留为 `ForkProcessManager`，`ThreadManager` 以同样的形态管理 Worker 线程。

```php
use Swow\Process\ProcessManager;
use Swow\Process\WorkerContext;

$manager = new ProcessManager(4);
$manager->start(static function (WorkerContext $context): void {
    echo "worker {$context->getId()} of {$context->getWorkerCount()} runs in process {$context->getPid()}\n";
    // Run a server here.
});
```

Passing a closure to the workers requires `swow.closure_serializer=1` in the PHP configuration; `startInline()` and `startScript()` work without it.

把闭包交给 Worker 需要在 PHP 配置中开启 `swow.closure_serializer=1`；`startInline()` 与 `startScript()` 不需要。

## 🍃 Coroutine-aware MongoDB driver | 协程化的 MongoDB 驱动

**A MongoDB query used to block the whole process. It no longer does.**

**过去一次 MongoDB 查询会阻塞整个进程。现在不会了。**

Build Swow with `--enable-swow-mongodb` and the I/O of the official `mongodb` extension becomes asynchronous on the Swow event loop. A slow query suspends only the coroutine that issued it, and the others keep running. Your application code and your MongoDB library stay the same.

使用 `--enable-swow-mongodb` 构建 Swow 后，官方 `mongodb` 扩展的 I/O 将在 Swow 的事件循环上异步进行。慢查询只挂起发起它的那个协程，其它协程继续运行。应用代码和所用的 MongoDB 库都不需要修改。

```php
use MongoDB\Driver\Command;
use MongoDB\Driver\Manager;
use Swow\Coroutine;

use function Swow\Sync\waitAll;

// The two commands are in flight at the same time, each on its own client.
foreach (['mongodb://127.0.0.1:27017', 'mongodb://127.0.0.1:27017'] as $uri) {
    Coroutine::run(static function () use ($uri): void {
        (new Manager($uri))->executeCommand('admin', new Command(['ping' => 1]))->toArray();
    });
}
waitAll();
```

## ⚠️ Upgrading | 升级须知

| Change | What to do | 变化 | 处理方式 |
| --- | --- | --- | --- |
| `Swow\Process\ForkProcess` is removed | Use `Swow\Process\ChildProcess::fork()` | 移除 `Swow\Process\ForkProcess` | 改用 `Swow\Process\ChildProcess::fork()` |
| `ProcessManager` is now spawn-based | Use `ForkProcessManager` to keep the fork-based behavior | `ProcessManager` 改为基于 spawn | 需要原先基于 fork 的行为时使用 `ForkProcessManager` |
| The numeric values of `Swow\Http\Parser::EVENT_*` changed with llhttp 9.4.3 | Use the constants and do not store their values | `Swow\Http\Parser::EVENT_*` 的数值随 llhttp 9.4.3 变化 | 使用常量本身，不要保存它们的数值 |
| Swow Library requires Swow extension `^2.0.0` | Update the extension together with the library | Swow Library 要求 Swow 扩展 `^2.0.0` | 扩展与 Library 一起更新 |
| The build requires OpenSSL 1.1.1 or later | Update OpenSSL before building | 构建要求 OpenSSL 1.1.1 或更高版本 | 构建前更新 OpenSSL |

<details>
<summary><b>Full changelog | 完整变更列表</b> (49 entries | 49 条)</summary>

## 🐣 What's New | 什么是牛的

+ Add `Swow\Thread` to run a callable or a piece of PHP code (`Thread::runCode()`) on an OS thread with its own PHP request; requires a ZTS build<br>
  新增 `Swow\Thread`，在拥有独立 PHP 请求的系统线程中运行 callable 或一段 PHP 代码（`Thread::runCode()`），需要 ZTS 构建 (ec1474c0, 3740d493) [@dixyes]
+ Add `Swow\Thannel`, a channel that works across threads, with `pushMany()`, `popMany()` and `drain()` batch operations<br>
  新增可跨线程使用的通道 `Swow\Thannel`，支持 `pushMany()`、`popMany()`、`drain()` 批量操作 (f8be2243, 0b360857) [@dixyes]
+ Add cross-thread synchronization primitives `Swow\Mutex`, `Swow\Semaphore`, `Swow\Futex` (with atomic `add()`, `exchange()` and `compareExchange()`) and `Swow\Sync\ThreadBarrier`<br>
  新增跨线程同步原语 `Swow\Mutex`、`Swow\Semaphore`、`Swow\Futex`（带原子操作 `add()`、`exchange()`、`compareExchange()`）与 `Swow\Sync\ThreadBarrier` (f8dd720f, 538da0a9, ef83f7cd, 83667687, 09624fce, c5dabefe) [@dixyes]
+ Add `Swow\SharedBuffer`, a typed shared memory region with anonymous and named mappings, and `Swow\SpscQueue`, a single-producer single-consumer queue on shared memory<br>
  新增带类型读写的共享内存区域 `Swow\SharedBuffer`（支持匿名与具名映射），以及基于共享内存的单生产者单消费者队列 `Swow\SpscQueue` (f1e9752b, 6e5f6e89, 3a95ac4e, ca28dfe3) [@dixyes]
+ Add `Swow\ThreadManager` to Swow Library to supervise worker threads with closure and inline modes, restart, reload and signal handling<br>
  Swow Library 新增 `Swow\ThreadManager`，以闭包或内联模式管理 Worker 线程，支持重启、重载与信号处理 (c1cb2d4f) [@dixyes]
+ Add the `swow.thread_shutdown_timeout` INI option to bound how long request shutdown waits for child threads<br>
  新增 INI 配置 `swow.thread_shutdown_timeout`，限制请求关闭时等待子线程的时长 (dcf30dae, c26ef7bf) [@dixyes]
+ Export the C interface of the thread facilities and install its headers, so other extensions can use them<br>
  导出线程设施的 C 接口并安装对应头文件，供其它扩展使用 (2ccc0ddb) [@dixyes]
+ Add `Swow\Process\ChildProcess` to spawn commands with configurable stdio and extra file descriptors, plus signal constants and a namespaced `Swow\Process\kill()` function<br>
  新增 `Swow\Process\ChildProcess`，可配置标准输入输出和额外文件描述符来启动命令，并提供信号常量与命名空间函数 `Swow\Process\kill()` (09838eab, ecab6139, 93fd2c16) [@dixyes]
+ Add a spawn-based `ProcessManager` that also works on Windows; the previous fork-based implementation is kept as `ForkProcessManager`, and both implement `ProcessManagerInterface`<br>
  新增基于 spawn 的 `ProcessManager`，可在 Windows 上工作；原先基于 fork 的实现保留为 `ForkProcessManager`，两者都实现 `ProcessManagerInterface` (805b50a7, 9fe5d3ed, 7fb96f23) [@dixyes]
+ Add an opt-in cooperative MongoDB runtime integration (`--enable-swow-mongodb`), so the `mongodb` extension no longer blocks the scheduler<br>
  新增可选启用的 MongoDB 协程化运行时集成（`--enable-swow-mongodb`），使 `mongodb` 扩展不再阻塞调度器 (3b62d95a, 71efbc6e) [@twosee]
+ Support `reneg_limit`, `reneg_window` and `reneg_limit_callback` for TLS renegotiation on PHP server streams<br>
  PHP 服务端流支持 TLS 重协商的 `reneg_limit`、`reneg_window` 与 `reneg_limit_callback` (a6d98c85, 68a4b94d) [@twosee]
+ Add `mandelbrot`, `gemm`, `transformer` and `fastext` examples that show multi-core computing with threads<br>
  新增 `mandelbrot`、`gemm`、`transformer`、`fastext` 示例，演示用线程做多核运算 (27416f50, f654c814, 4181ddb1, 085b40f3) [@dixyes]

## ✨ What's Enhanced | 加强了啥

太多啦，写不完。

+ Adopt the PHP 8.6 and 8.7 internal API changes, including the `__thread` based `EG()` and `CG()` of php-src 8.7<br>
  适配 PHP 8.6 与 8.7 的内部 API 变化，包括 php-src 8.7 基于 `__thread` 的 `EG()` 与 `CG()` (3b32eba7, 92a2c0d1, e0556ced, e6297b8f, de9e9f24) [@dixyes]
+ Sync the hooked cURL, filesystem, PostgreSQL and OpenSSL code with upstream PHP, and add the missing cURL constants<br>
  将被 hook 的 cURL、文件系统、PostgreSQL、OpenSSL 代码与上游 PHP 同步，并补齐缺失的 cURL 常量 (86fe1113, 750e4c29, bba7e2a6, 8b5d57ac, 6094f1ae, fc273e7e, d29b3e9b) [@dixyes]
+ Update llhttp from 9.2.1 to 9.4.3: `Swow\Http\Parser` gains `EVENT_PROTOCOL`, and the numeric values of the other `EVENT_*` constants change, so use the constants and do not store their values<br>
  llhttp 从 9.2.1 升级到 9.4.3：`Swow\Http\Parser` 新增 `EVENT_PROTOCOL`，其它 `EVENT_*` 常量的数值随之变化，请使用常量本身，不要保存它们的数值 (3b4810db) [@dixyes]
+ Support OpenSSL 4 and require OpenSSL 1.1.1 or later in the build<br>
  支持 OpenSSL 4，构建要求 OpenSSL 1.1.1 或更高版本 (370eb7b6, 579f29b4, a388e68f, eeb98537) [@twosee] [@dixyes]
+ Build HTTP messages in one pass and reject CR and LF in request lines, status lines, header names and header values<br>
  HTTP 报文改为单次构建，并拒绝请求行、状态行、头名称与头值中的 CR 和 LF (eb9d04f2, 7979cdf9) [@dixyes]
+ Retain cURL connection caches across successive easy transfers<br>
  在连续的 cURL easy 传输之间保留连接缓存 (8d5c950a) [@twosee]
+ Align hooked stream, TLS and cURL error messages with native PHP, including peer verification diagnostics and version-specific cURL multi errors<br>
  让被 hook 的流、TLS 与 cURL 的错误信息与原生 PHP 保持一致，包括对端验证诊断信息和随 PHP 版本变化的 cURL multi 错误 (a0edfe5e, 40962800, 550db32e, eab87f9d, 9471f4b2, e8fc7dbc, 94af05ba) [@twosee]
+ Validate ALPN protocol names, certificate paths and peer fingerprint types before enabling stream crypto<br>
  启用流加密前校验 ALPN 协议名、证书路径与对端指纹类型 (7ecae67a, a49a3776, 7d23cbc7) [@twosee]
+ Validate SOCKS5 authentication replies and encode IPv6 hosts as address literals in the HTTP client proxy support<br>
  HTTP 客户端代理支持中校验 SOCKS5 认证应答，并把 IPv6 主机编码为地址字面量 (86fa446f, 6fdc2f3e, c668f0fa) [@twosee]
+ Support ThreadSanitizer builds<br>
  支持 ThreadSanitizer 构建 (601897c6) [@dixyes]
+ Update the stub docblocks to match runtime behavior and document the process and thread APIs<br>
  更新 stub 文档注释使其与运行时行为一致，并为进程与线程 API 补充文档 (880579e7, b4e2d939, 35ca16b9, 5c921619) [@dixyes]

## 🐛 What's Fixed | 什么修了

一大堆大的小的有的没的 Bug，建议所有人更新，不更新会有蓝色大肥鱼搬走你家米缸。

* Fix socket and HTTP parser coredumps, out-of-bounds reads in the WebSocket helpers, and the buffer copy order in a socket read<br>
  修复 socket 与 HTTP 解析器的崩溃、WebSocket 辅助函数的越界读取，以及 socket 读取时缓冲区拷贝的顺序 (18976c7e, aa4e277f, a88cbacd, c0ada709) [@dixyes]
* Fix exception handling around `defer`: pending exceptions are isolated while callbacks run, execution stops after a deferred fatal error, and a delayed OOM propagates across coroutine boundaries<br>
  修复 `defer` 相关的异常处理：运行回调时隔离待处理异常，延迟致命错误后不再继续执行，延迟的 OOM 会跨协程边界传播 (7bc0aa57, 74cb49fc, ca68b3ee) [@twosee]
* Fix cURL hook lifetime issues: cleared callback handlers, callback receivers during teardown, temporary callback return values, and persistent share handlers before PHP 8.3<br>
  修复 cURL hook 的生命周期问题：已清除的回调处理器、销毁过程中的回调接收者、回调的临时返回值，以及 PHP 8.3 之前的持久化 share 句柄 (fc8fcd2e, 3aff2ced, 8348af54, fde17815, 9d201773) [@twosee]
* Fix TLS stream behavior: wait for the TCP connection before a blocking handshake, keep the connection timeout for manual handshakes, avoid blocking supplemental reads, and preserve verification stores across SNI selection<br>
  修复 TLS 流的行为：阻塞握手前先等待 TCP 连接完成，手动握手时保留连接超时，补充读取不再阻塞，SNI 选择后保留证书验证存储 (1e4c0cc2, 845f78a5, 552943fa, 5efef1b8, f950b534, 204bcff3) [@twosee]
* Fix `Socket::enableCrypto()` TypeError when null options are passed explicitly<br>
  修复显式传入 null 选项时 `Socket::enableCrypto()` 抛出 TypeError (5637bab9) [@PandaLIU-1111]
* Fix the PostgreSQL hook: restore `Pdo\Pgsql`, drain unbuffered results before clearing their state, take over a running lazy fetch before buffered executes, and resize the column description of a re-executed statement<br>
  修复 PostgreSQL hook：恢复 `Pdo\Pgsql`，清理状态前先读完未缓冲的结果，缓冲执行前接管正在进行的惰性读取，重新执行语句时调整列描述的大小 (8a3b9d5b, 656922a3, 3be876a6, 7bf99f09, 8605b18e) [@dixyes] [@twosee]
* Fix HTTP message reception in Swow Library: header fragments across receive buffers, chunk trailers, close-delimited bodies, HEAD responses, informational responses, pipelined request boundaries and repeated header values<br>
  修复 Swow Library 的 HTTP 报文接收：跨接收缓冲区的头片段、chunk trailer、以连接关闭为结束的 body、HEAD 响应、1xx 响应、管线化请求的边界，以及重复的头值 (e9b76bde, 34688ea9, 0a6254f2, ab908072, a4fa628c, 68a8b6ae, 249257dd, 554fb0f1) [@twosee]
* Fix HTTP timeouts and connection reuse: apply timeouts to every body receive path, and stop reusing a connection after a protocol limit failure<br>
  修复 HTTP 超时与连接复用：超时作用于所有 body 接收路径，协议限制失败后不再复用连接 (1c82c56d, 77f678bd, 30860fa5, 0df6bb33, 73d16a0f) [@twosee]
* Fix SSE parsing (line endings, field parsing, event ID retention) and WebSocket handling (empty frame unmasking, ping payload echo)<br>
  修复 SSE 解析（行结束符、字段解析、事件 ID 保留）与 WebSocket 处理（空帧的解除掩码、pong 回显 ping 负载） (383932d6, 6a186921, 77320b76, 7db7780a) [@twosee]
* Fix PSR-7 message details: zero-valued bodies, XML body field names and escaping, protocol versions of converted server requests, URI changes on new requests, and upload file ownership when reception fails<br>
  修复 PSR-7 报文细节：值为零的 body、XML body 的字段名与转义、服务端请求转换时的协议版本、新建请求上的 URI 变更，以及接收失败时上传文件的归属 (ad8d5205, 3e0889e5, 297912df, bd8634bd, e89c9772, 08e75cee, 30889c4f, 18f86d1a) [@twosee]
* Fix `EofStream` and `LengthStream`: timeouts of string receives, frames that follow the first one, and the advertised length of message slices<br>
  修复 `EofStream` 与 `LengthStream`：字符串接收的超时、首帧之后的后续帧，以及发送消息切片时声明的长度 (bb7dfffa, 7178c620, 5e697693, 0a694bdb) [@twosee]
* Fix closure serialization: line constants, static variables of an executed closure, by-reference uses, and ZTS builds<br>
  修复闭包序列化：行号常量、已执行闭包的静态变量、按引用 use，以及 ZTS 构建 (5675f383, 406bc88d, 154d8305, 13a0eed1, 1d003261) [@twosee] [@dixyes]
* Fix symbolic link and path queries on Windows<br>
  修复 Windows 上的符号链接与路径查询 (6f2eb278, 3549568b, e554b790, aede0fab) [@twosee]
* Fix process handling: `proc_close()` child handle double close, reaping a running child when closing a process, and worker readiness in ProcessManager<br>
  修复进程处理：`proc_close()` 重复关闭子进程句柄、关闭进程时回收仍在运行的子进程，以及 ProcessManager 的 Worker 就绪通知 (3febc7b2, db923aca, 9cf7eb1f, 51c01229) [@dixyes] [@twosee]
* Fix thread facility edge cases: mutex ownership on handover, semaphore release past the platform limit, barrier count after a timed out waiter, named mapping ownership checks, and serialized shared objects of another class or process<br>
  修复线程设施的边界情况：移交时的互斥锁归属、超过平台上限的信号量释放、等待者超时后的屏障计数、具名映射的归属检查，以及属于其它类或进程的已序列化共享对象 (d445430c, af0ee1dd, 15dddd64, e4cd3750, 21c44a01, 8dfd7666, 434a660d, b77e512b) [@dixyes] [@twosee]
* Fix debugger and watchdog issues: forced debug observation interrupt starvation, exceptions thrown by debug handlers, and the execution location captured on the watched thread<br>
  修复调试器与 watchdog 的问题：强制调试观察导致的中断饥饿、调试处理器抛出的异常，以及在被监视线程上采集执行位置 (849e62c5, 9f67fb71, 67b70d71, aa33c009, d79d6305) [@twosee] [@dixyes]
* Fix miscellaneous issues: CIDR mask arithmetic, `IpAddress::__construct()` null argument, constructor-less coroutine objects, persistent streams at runtime shutdown, user opcode handlers overwritten by coroutine hooks, selector push values, and a double free of the last error message<br>
  修复其它问题：CIDR 掩码运算、`IpAddress::__construct()` 的 null 参数、未调用构造函数的协程对象、运行时关闭时的持久化流、被协程 hook 覆盖的用户 opcode 处理器、selector 的 push 值，以及 last error 消息的重复释放 (115fec9d, 37f818a4, fc094d00, ff4aaaf1, 076d1d2c, f335601b, d6f4bc2f, 2ebeb433, 0b36e638) [@dixyes] [@twosee]

## 👻 What's Removed | 什么移除了

- Remove `Swow\Process\ForkProcess`; use `Swow\Process\ChildProcess::fork()` instead<br>
  移除 `Swow\Process\ForkProcess`，请改用 `Swow\Process\ChildProcess::fork()` (09838eab) [@dixyes]

## 📦 Internal | 内部

CI 测试矩阵改为 PHP 8.2 ~ master。

+ Run thread-safe CI jobs on Linux and macOS, and use atomic GCOV counters for threaded coverage<br>
  在 Linux 与 macOS 上运行线程安全的 CI 任务，线程覆盖率使用原子 GCOV 计数器 (9f609283, 08dd32b3, ed29d8c2) [@dixyes]
+ Add a large number of tests for cURL, HTTP/2, PostgreSQL, TLS streams, filesystem hooks, PSR-7, WebSocket, the debugger and ProcessManager<br>
  为 cURL、HTTP/2、PostgreSQL、TLS 流、文件系统 hook、PSR-7、WebSocket、调试器和 ProcessManager 添加大量测试 (3ef58ce9, 6ac83b9a, 2f558f06, 639e061d, a2d7e7cc, f014802e) [@twosee]
+ Use local servers and local TLS peers instead of public sites in tests<br>
  测试改用本地服务器与本地 TLS 对端，不再依赖公网站点 (540de0d9, af43c9c7) [@twosee]
+ Bound library test execution time on all platforms and fail release builds when required extension tests fail<br>
  限制各平台上 library 测试的执行时间，必需的扩展测试失败时让发布构建失败 (14abba82, 15778046, 07796eb5, e75aa3a7) [@twosee]
+ Add MongoDB coroutine benchmarks<br>
  新增 MongoDB 协程性能基准 (11969acb, c90555b6, d09b2fc3) [@twosee]
+ Pin the code style tooling and check the headerless examples in CI<br>
  固定代码风格工具的版本，并在 CI 中检查无文件头的示例 (a65695e6, a6a2ffa0, c4131815) [@twosee] [@dixyes]
+ Document feature branch validation before `ci` integration and translate contributor instructions into English<br>
  记录合入 `ci` 前的特性分支验证流程，并把贡献者说明翻译为英文 (7a97aefb, 879abcf4) [@twosee]
+ Sync libcat dependencies (multiple updates)<br>
  同步 libcat 依赖（多次更新） [@twosee] [@dixyes]

</details>

---

# v2.0.0-alpha.3

> Release Date: 2026-10-10 | 发布日期：2026-10-10

> "The bug only happened once, but it only needed to." —— AI
>
> 「这个 BUG 只出现过一次，但一次就够了。」 —— AI

### Version Highlights | 版本亮点

This alpha release adds socket recreation and TCP REUSEPORT load balancing for multi-process servers, refines PSR-7 keep-alive semantics, improves ProcessManager supervision, and fixes a curl hook use-after-free that could hang the event loop. Swow Library now requires Swow extension `>=2.0.0-alpha.3`.
该 alpha 版本为多进程服务新增 socket 重建与 TCP REUSEPORT 负载均衡，完善 PSR-7 keep-alive 语义，改进 ProcessManager 进程监管，并修复 curl hook 中可能导致事件循环卡死的释放后使用问题。Swow Library 现在要求 Swow 扩展版本 `>=2.0.0-alpha.3`。

## 🐣 What's New | 什么是牛的

+ Add `Socket::recreate()` to rebuild the native socket while keeping the PHP object | 新增 `Socket::recreate()`，在保留 PHP 对象的同时重建底层 socket (7877fa2e, bdf86d13) [@twosee]
+ Add `Socket::supportsTcpReusePortLoadBalancing()` and let multi-process `EventDriver` listen per worker with REUSEPORT when supported | 新增 `Socket::supportsTcpReusePortLoadBalancing()`，多进程 `EventDriver` 在平台支持时改为每个 Worker 独立 REUSEPORT 监听 (1ae90cc2, 17373b3f) [@twosee]
+ Add `ProcessManager::onWorkerExit()` to receive worker id, pid and exit status | 新增 `ProcessManager::onWorkerExit()`，可获取 Worker 编号、pid 与退出状态 (9887b067) [@twosee]
+ Add explicit socket access, rebind helpers and SSE/WebSocket send helpers to PSR-7 Client and Server | 为 PSR-7 Client 与 Server 增加显式 socket 访问、rebind 辅助方法以及 SSE/WebSocket 发送辅助方法 (2ebf06bc) [@twosee]

## ✨ What's Enhanced | 加强了啥

+ Refactor PSR-7 Client, Server and ServerConnection to compose inner Socket objects | 重构 PSR-7 Client、Server 与 ServerConnection，改为组合内部 Socket 对象 (2ebf06bc) [@twosee]
+ Add a fast path for single-token Connection header parsing | 为单 token 的 Connection 头解析增加快速路径 (24a57b8e) [@twosee]
+ Improve ProcessManager reload supervision so reloads run in the background and replaced workers are watched correctly | 改进 ProcessManager 重载监管，重载改为后台执行，新 Worker 能被正确监控 (61311d3a) [@twosee]
+ Make ProcessManager shutdown wait only for its own coroutines and enforce the worker shutdown timeout | ProcessManager 关闭时只等待自身创建的协程，并强制执行 Worker 关闭超时 (9887b067) [@twosee]
+ Narrow Composer `replace` constraints to the actually bundled sub-package versions | 将 Composer `replace` 约束收紧为实际内置的子包版本 (45b3e681) [@twosee]

## 🐛 What's Fixed | 什么修了

* Fix curl hook use-after-free when libcurl removes a socket during `socket_action`, which could spin the event loop forever | 修复 libcurl 在 `socket_action` 期间移除 socket 时 curl hook 的释放后使用问题，该问题可能导致事件循环无限空转 (023522f9) [@twosee]
* Fix PSR-7 keep-alive semantics to follow the protocol version and Connection header of both request and response | 修复 PSR-7 keep-alive 语义，使其遵循请求与响应的协议版本及 Connection 头 (9c6b8699) [@twosee]
* Fix stub generation for interfaces and abstract methods | 修复接口与抽象方法的 stub 生成 (5e79110a) [@twosee]

## 📦 Internal | 内部

+ Prefer local Swow builds in php-wrapper and make the `dontdie` script executable | php-wrapper 优先加载本地 Swow 构建，并为 `dontdie` 脚本添加可执行权限 (5e79110a, 61af2b81) [@twosee]

---

# v2.0.0-alpha.2

> Release Date: 2026-05-28 | 发布日期：2026-05-28

> "CI green is a lifestyle, not a destination." —— AI
>
> 「CI 全绿是一种生活方式，不是一个终点。」 —— AI

### Version Highlights | 版本亮点

Maintenance release focusing on CI stability, libcat socket/SSL improvements, and macOS test compatibility.
维护版本，专注于 CI 稳定性、libcat socket/SSL 改进以及 macOS 测试兼容性。

## ✨ What's Enhanced | 加强了啥

+ Sync libcat with socket API improvements and SSL enhancements | 同步 libcat socket API 改进与 SSL 增强 (61f99236) [@twosee]

## 🐛 What's Fixed | 什么修了

* Fix Windows CI: add brotli/zstd dependencies for PHP 8.2+ builds | 修复 Windows CI：为 PHP 8.2+ 构建添加 brotli/zstd 依赖 (cb24a114) [@twosee]
* Fix PHPUnit 11 CoversClass annotation warnings causing test failures | 修复 PHPUnit 11 CoversClass 注解告警导致的测试失败 (cb24a114) [@twosee]
* Fix code style compatibility with php-cs-fixer v3.95 | 修复与 php-cs-fixer v3.95 的代码风格兼容问题 (b8f0c716) [@twosee]
* Skip curl hook tests on macOS due to known async DNS issue | 跳过 macOS 上的 curl hook 测试（已知异步 DNS 问题）(16916ff8) [@twosee]
* Skip watchdog syscall detection test on macOS (unreliable) | 跳过 macOS 上的 watchdog 系统调用检测测试（不稳定）(16916ff8) [@twosee]

## 📦 Internal | 内部

+ Update CI test infrastructure for broader compatibility | 更新 CI 测试基础设施以提升兼容性 (cb24a114, 16916ff8) [@twosee]

---

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
