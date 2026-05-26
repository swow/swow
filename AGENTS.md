# AGENTS.md

## 项目结构

- `ext/deps/libcat/` 是从 libcat 源库同步过来的，禁止直接修改
- libcat 源库路径：`/Users/twosee/Toast/cat/libcat/`（与 swow 同级）
- 同步流程：libcat 提交并 push → swow 项目下执行 `composer sync-dependencies` → 用脚本输出的信息提交
- 同步 commit 格式：`Sync deps: libcat\n\n* libcat/libcat@<hash>`

## 协程规则

- 所有 `Coroutine::run()` 创建的协程必须保存引用到对象属性，禁止空悬（fire-and-forget）
- 对象 `stop()`/销毁时必须确保所有协程退出：自然退出的确保前置条件满足，阻塞中的主动 `$coroutine->kill()`
- kill 时排除 `Coroutine::getCurrent()`（stop 可能从被管协程内部调用）
- 使用 `waitAll()` 前必须保证所有协程最终可退出，否则死锁

## 多进程

- 采用 Prefork 模式：父进程 bind+listen，fork 后子进程继承 fd 直接 accept
- 不要用 `SO_REUSEPORT` — macOS 上 libuv 不实际设置该选项
- fork 后事件循环由 `cat_event_fork()` 自动重建，已有 handle 继续有效
