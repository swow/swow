# AGENTS.md

## Language

- Write all code comments in English, including comments in source files, tests, examples, scripts, and workflow snippets.
- Keep this `AGENTS.md` entirely in English.
- Swow follows the same English-comment requirement as libcat. This project rule takes precedence over any general preference for Chinese comments; that preference applies to other projects.

## Project Structure

- `ext/deps/libcat/` is synchronized from the sibling libcat source repository. Do not edit it directly. The source repository is `/Users/twosee/Toast/cat/libcat/`.
- To synchronize libcat, commit the libcat changes first, then run `composer sync-dependencies` in Swow. Use the commit message `Sync deps: libcat\n\n* libcat/libcat@<hash>`.
- Swow is a monorepo. `tools/split.php` publishes its packages: `ext/` -> `swow/swow-extension`, `lib/swow-library/` -> `swow/swow-library`, `lib/swow-library/lib/psr7-plus/` -> `swow/psr7-plus`, and `lib/swow-stub/` -> `swow/swow-stub`.
- The split process pushes only to the package repositories' `develop` branches. Stable releases also require separate tags and releases. The script performs remote operations and pushes; do not run it without explicit user authorization.

## Versions and Generated Files

- `tools/versions.php` is the source of version information. Do not manually edit version constants in `Swow\Extension`, `Swow\Library`, `ext/include/swow.h`, or stubs.
- `Swow\Library::REQUIRED_EXTENSION_VERSION` is generated from `swow-library.required_extension_version` in `tools/versions.php`.
- `composer prepare-release` and `composer update-version` synchronize the extension, library, stub, and other generated version files.
- `lib/swow-stub/src/Swow.php` is generated. When adding an extension API, update the C implementation first, then run `composer gen-stub`; do not edit only the stub.

## Extension Constraints

- Composer packages should normally list `ext-swow` under `suggest`. Do not make it a `require` by default.
- `lib/swow-library/src/bootstrap.php` already checks the extension version by comparing `Swow\Extension::VERSION` with `Swow\Library::REQUIRED_EXTENSION_VERSION`.
- When PHP code starts using a new C extension API, also publish a `swow/swow-extension` version containing that API. Set `required_extension_version` in `tools/versions.php` to the first extension version that provides it.
- Do not hide missing APIs behind overly broad version constraints. For example, if an API was added after a particular alpha release, `^2.0` may still accept earlier alphas without that API.
- Preserve synchronous behavior in the plain-filesystem `file_exists()` and `realpath()` fast paths. Adding implicit suspension can break autoloading inside `Coroutine::call()` and code that observes a coroutine's first suspension.

## Coroutines and Multiple Processes

- Retain references to long-running coroutines created by `Coroutine::run()`. When stopping an object, ensure its associated coroutines exit. Exclude `Coroutine::getCurrent()` when explicitly killing coroutines.
- Before calling `waitAll()`, ensure every participating coroutine can eventually exit; otherwise it will deadlock.
- Use prefork as the default multi-process model: the parent binds and listens, then forked children inherit the file descriptor and accept connections.
- Select platform-specific behavior using actual capabilities exposed by libcat, libuv, or the extension, rather than only checking the operating system name in PHP.
- Keep the shared-file-descriptor path on unsupported or unverified platforms. On platforms supporting independent listeners, also handle ephemeral ports, inherited socket options, and worker readiness.

## API Design Boundaries

- High-level PSR-7 Server, Client, and Connection objects should express protocol semantics rather than mechanically expose every low-level Socket read/write method.
- When rebuilding a native socket, preserve PHP object state where possible. Explicitly identify which native socket options must be recorded and reapplied.
- Put reusable protocol helpers in pure PHP packages instead of coupling them to ext-swow or a particular application framework.
- External framework adapters should only adapt interfaces. Do not move general protocol behavior into a framework-specific layer or rely on downstream framework patches to correct Swow behavior.

## Composer Dependencies

- `replace` means that a package already contains another package. Do not use `replace: "*"` to claim that it contains every future version. Specify the version range actually included.

## Git Branches and Releases

- `develop` is the production branch and `ci` is the development branch. Keep linear history; do not create merge commits.
- Keep `ci` rebased on top of `develop`. The same policy applies to libcat.
- Release sequence: run `composer cs-fix`, commit `Fix code style`, update `tools/versions.php`, run `composer prepare-release`, commit `Release vX.Y.Z`, verify that `develop` is an ancestor of `ci`, then fast-forward `develop`.
- Write commit messages as concise English imperative sentences without Conventional Commit prefixes. Common forms include `Add ...`, `Fix ...`, `Improve ...`, `Update ...`, `Remove ...`, and `Support ...`.
- Before pushing, run `composer cs-fix` with the version of php-cs-fixer locked by the project so the coding-style check does not fail first in CI.
