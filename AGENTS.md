# AGENTS.md

## 项目结构

- `ext/deps/libcat/` 从同级 libcat 源库同步，禁止直接修改；源库路径为 `/Users/twosee/Toast/cat/libcat/`
- libcat 同步流程：先提交 libcat，再在 swow 执行 `composer sync-dependencies`，提交信息使用 `Sync deps: libcat\n\n* libcat/libcat@<hash>`
- Swow 是 monorepo，子包由 `tools/split.php` 发布：`ext/` → `swow/swow-extension`，`lib/swow-library/` → `swow/swow-library`，`lib/swow-library/lib/psr7-plus/` → `swow/psr7-plus`，`lib/swow-stub/` → `swow/swow-stub`
- split 只推子仓库 `develop`，稳定版本还需要单独 tag / release；split 会执行 remote / push，未经用户确认不要运行

## 版本与生成物

- 版本源头是 `tools/versions.php`，不要直接手改 `Swow\Extension`、`Swow\Library`、`ext/include/swow.h`、stub 中的版本常量
- `Swow\Library::REQUIRED_EXTENSION_VERSION` 由 `tools/versions.php` 的 `swow-library.required_extension_version` 生成
- `composer prepare-release` / `composer update-version` 会同步扩展、library、stub 等版本生成物
- `lib/swow-stub/src/Swow.php` 是生成文件；新增扩展 API 后先更新 C 实现，再运行 `composer gen-stub`，不要只手改 stub

## 扩展约束

- Composer 包里 `ext-swow` 通常只能作为 `suggest`，不要默认改成 `require`
- `lib/swow-library/src/bootstrap.php` 已内置扩展版本检测，用 `Swow\Extension::VERSION` 对比 `Swow\Library::REQUIRED_EXTENSION_VERSION`
- PHP 代码开始依赖新的 C 扩展 API 时，必须同时发布包含该 API 的 `swow/swow-extension`，并在 `tools/versions.php` 收紧 `required_extension_version` 到首个包含该 API 的扩展版本
- 不要用过宽约束掩盖 API 缺失；例如新 API 只存在于某个 alpha 之后时，`^2.0` 这类约束可能仍接受更早 alpha

## 协程与多进程

- `Coroutine::run()` 创建的长期协程必须保存引用，禁止空悬；停止对象时要保证相关协程退出，主动 kill 时排除 `Coroutine::getCurrent()`
- 使用 `waitAll()` 前必须确认所有参与协程最终可退出，否则会死锁
- 多进程默认采用 prefork 思路：父进程 bind/listen，fork 后子进程继承 fd accept
- 需要平台能力分流时，以 libcat/libuv 或扩展暴露的真实能力为准，不要只靠 PHP 层 OS 名称判断
- 不支持或无法确认能力的平台保留共享 fd 路径；支持独立监听的平台也要处理随机端口、socket 选项继承和 worker ready 这类进程间边界

## API 设计边界

- PSR-7 Server、Client、Connection 这类高层对象优先表达协议语义，不要机械暴露底层 Socket 的全部读写方法
- 需要重建 native socket 时，优先保留 PHP 对象状态；同时明确哪些 native socket 选项需要记录和重放
- 公共协议 helper 应放在可被复用的纯 PHP 包中，避免依赖 ext-swow 或具体上层框架
- 外部框架适配层只做适配，不把通用协议语义塞进引擎专用包；Swow 自身行为也不能依赖下游框架修补

## Composer 依赖

- `replace` 表示「这个大包已经包含某个子包，可以当作那个子包用」。不要写 `replace: "*"` 表示永远包含所有未来版本；应写大包实际内置的子包版本范围

## Git 分支与发布

- `develop` = 生产，`ci` = 开发；线性历史，禁止 merge commit
- ci 必须始终 rebase 在 develop 之上；libcat 同策略
- 发布流程：`composer cs-fix` → 提交 `Fix code style` → 改 `tools/versions.php` → `composer prepare-release` → 提交 `Release vX.Y.Z` → 确认 `develop` 是 `ci` 祖先 → fast-forward 到 develop
- Commit 信息使用简洁祈使句，不用 conventional commit 前缀；常见格式为 `Add ...`、`Fix ...`、`Improve ...`、`Update ...`、`Remove ...`、`Support ...`
- 推送前运行 `composer cs-fix`，使用项目锁定的 php-cs-fixer 版本，避免 CI 先在代码风格阶段失败
