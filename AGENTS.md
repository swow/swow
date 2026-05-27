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

## Git 分支与发布

- `develop` = 生产，`ci` = 开发；线性历史，禁止 merge commit
- ci 必须始终 rebase 在 develop 之上
- libcat（`~/Toast/cat/libcat/`）同策略

### 发布流程

```bash
# 1. cs-fix + commit
composer cs-fix && git add -A && git commit -m "Fix code style"

# 2. 改 tools/versions.php（version + required_extension_version）

# 3. prepare-release + commit
composer prepare-release && git add -A && git commit -m "Release vX.Y.Z"

# 4. 推送前确认 develop 是 ci 祖先
git merge-base --is-ancestor develop ci

# 5. 推 ci → ff develop
git push origin ci
git checkout develop && git merge ci --ff-only && git push origin develop && git checkout ci

# 6. libcat 同理（如有变更）
```

### Commit 风格

简洁祈使句，不用 conventional commit 前缀：
- `Fix code style` / `Release vX.Y.Z` / `Sync deps: libcat`
- `Add ...` / `Fix ...` / `Improve ...` / `Update ...` / `Remove ...` / `Support ...`
- 不要写 `feat:` / `style:` / `chore:` 等前缀
