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
- `ext/include/cat_vm.h` maps `cat_malloc`, `cat_calloc`, `cat_realloc` and `cat_free` (and helpers built on them such as `cat_strdup`) to the Zend allocator (`emalloc` and friends), which is bound to the PHP request thread. Code running on a libuv or `cat_work()` worker thread, and any buffer handed between a worker and the PHP thread, must use `cat_sys_malloc`, `cat_sys_realloc`, `cat_sys_strdup` and `cat_sys_free`; the same applies to `zend_string`, `zval` and other Zend APIs, which must stay on the PHP thread. Mixing them crashes ZTS builds and may pass unnoticed on NTS, so verify such code on a ZTS build.

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
- For an authorized integration, push and validate the feature branch first. Only after its GitHub Actions checks pass, integrate it linearly into `ci`, push `ci`, and delete the local and remote feature branches. Keep the feature branch while validation or integration is incomplete; never infer push authorization from this workflow alone.
- Release sequence: update `tools/versions.php`, run `composer prepare-release`, commit `Release vX.Y.Z`, verify that `develop` is an ancestor of `ci`, then fast-forward `develop`. See "Release Procedure" below for the full checklist.
- `Fix code style` is a fallback commit for style errors that slipped through. When the coding-style check is green, do not add it before a release.
- Write commit messages as concise English imperative sentences without Conventional Commit prefixes. Common forms include `Add ...`, `Fix ...`, `Improve ...`, `Update ...`, `Remove ...`, and `Support ...`.
- Before pushing, run `composer cs-fix` with the version of php-cs-fixer locked by the project so the coding-style check does not fail first in CI.

## Release Procedure

Every step that pushes a branch or tag, publishes a release, or runs `composer split` needs explicit user authorization for that step.

1. Rehearse the Windows build: run the `releases` workflow through `workflow_dispatch` on the commit to be released. This builds every DLL but skips the `Upload DLLs` job, which only runs on tag pushes, so the upload itself is first exercised by the real tag.
2. Prepare the `Release vX.Y.Z` commit on a feature branch. It contains only the new `CHANGELOG.md` entry and the generated version files (`tools/versions.php`, `ext/include/swow.h`, `lib/swow-library/src/Library.php`, `lib/swow-stub/src/Swow.php`). Review README and CHANGELOG wording in separate commits before it.
3. Push the feature branch and wait until every check on the release commit is green, including `codecov/project`. Then fast-forward `ci` and `develop` to it.
4. Push the lightweight tag `vX.Y.Z` on the release commit. The `releases` workflow builds the DLLs and creates a draft pre-release whose body is a "Hashes and notes" table.
5. Edit that draft: put the CHANGELOG entry above the generated table, clear the pre-release flag for stable versions, mark it as latest, and publish it.
6. Run `composer split`. It needs `splitsh-lite` on `PATH` and force-pushes each package repository's `develop`; compute the split hashes first and confirm that every push is a fast-forward or a no-op.
7. Tag the split commits in the package repositories with lightweight tags and create their releases: `swow-extension`, `swow-library`, and `swow-stub` follow the Swow version, while `psr7-plus` has its own version line. The release body of a package that follows the Swow version is `See https://github.com/swow/swow/releases/tag/vX.Y.Z for detail`.
8. Confirm that Packagist lists the new versions of `swow/swow` and the split packages.
9. Set the next `-dev` version in `tools/versions.php`, run `composer update-version`, and commit `Back to dev` on `ci` only. `develop` stays on the release commit.

Notes learned from past releases:

- The `tests` workflow cancels a branch's in-progress run when a newer commit is pushed. A cancelled run uploads partial coverage, and Codecov then uses that commit as an inflated base, so the next commit can fail `codecov/project` without any real drop. Before a release, let each pushed commit finish its run instead of pushing several commits in a row.
- Codecov keeps the comparison it computed when a commit was first processed; re-running jobs on the base or head commit does not refresh it. If the release commit is still only on an unshared feature branch, recreate it with a new hash (for example `git commit --amend --no-edit`) after the base commit has complete coverage.
- CHANGELOG entries mention contributors by GitHub login, and the release body turns them into mentions. A Git author name is not a login: resolve each contributor from the pull request or from `gh api repos/swow/swow/commits/<hash> -q .author.login`, and do this before the `Release vX.Y.Z` commit is created. After the tag is pushed, the CHANGELOG inside the tag can no longer be corrected; only the release body and later commits can.
- Write every CHANGELOG entry in the format of the v2.0.0 entry: release date, quote, "Version Highlights" with a bold tagline, a bilingual summary and an "at a glance" table, one section per headline feature with a tagline, an explanation and a code sample or table, an "Upgrading" table, and the full list inside a collapsed `<details>` block whose entries put English and Chinese on separate lines followed by commit hashes and contributor logins.
- `uploader.ps1` appends the hash table to the existing release body and posts the whole body back. Before re-running the `releases` workflow on a published release, back up the body and delete the old assets and the old hash table; assets with the same name are not replaced.
- README translations (`README-CHS.md`, `README-CHT.md`, `README-JP.md`, `README-PT.md`, `README-RU.md`) mirror `README.md`. Change the feature table and the language switcher in all of them together, and translate by meaning rather than word for word.
