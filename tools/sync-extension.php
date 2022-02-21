#!/usr/bin/env php
<?php
/**
 * This file is part of Swow
 *
 * @link     https://github.com/swow/swow
 * @contact  twosee <twosee@php.net>
 *
 * For the full copyright and license information,
 * please view the LICENSE file that was distributed with this source code
 */

declare(strict_types=1);

use Swow\Util\FileSystem;
use function Swow\Util\defer;
use function Swow\Util\error;
use function Swow\Util\log;
use function Swow\Util\notice;
use function Swow\Util\ok;

require __DIR__ . '/autoload.php';

$workSpace = __DIR__ . '/../ext/deps';

$sysTempDir = (static function (): string {
    if (is_writable('/tmp')) {
        return '/tmp';
    }
    $tempDir = sys_get_temp_dir();
    notice('Sys temp dir is redirected to ' . $tempDir);
    return $tempDir;
})();

$sync = static function (string $name, string $url, string $sourceDir, string $targetDir, array $requires = []) use ($sysTempDir): string {
    if (PHP_OS_FAMILY === 'Windows') {
        throw new RuntimeException('Linux only');
    }
    $targetWorkspace = dirname($targetDir);
    if (file_exists($targetDir) && !shell_exec("cd {$targetWorkspace} && git rev-parse HEAD")) {
        throw new RuntimeException("Unable to find git repo in {$targetDir}");
    }
    $tmpBackupDir = $sysTempDir . '/swow_deps_backup_' . date('YmdHis');
    if (!is_dir($tmpBackupDir) && !mkdir($tmpBackupDir, 0755)) {
        throw new RuntimeException("Unable to create tmp backup dir '{$tmpBackupDir}'");
    }
    if (str_ends_with($url, '.git') || (parse_url($url)['scheme'] ?? '') === 'file') {
        $tmpSourceDir = $sysTempDir . "/swow_deps/{$name}";
        if (is_dir($tmpSourceDir)) {
            FileSystem::remove($tmpSourceDir);
        }
        if (!mkdir($tmpSourceDir, 0755, true)) {
            throw new RuntimeException("Unable to create tmp source dir '{$tmpSourceDir}'");
        }
        defer($defer, static function () use ($tmpSourceDir): void {
            if (is_dir($tmpSourceDir)) {
                FileSystem::remove($tmpSourceDir);
            }
        });
        shell_exec("git clone --depth=1 {$url} {$tmpSourceDir}");
        if (!is_dir("{$tmpSourceDir}/.git")) {
            throw new RuntimeException("Clone source files of {$name} failed");
        }
        $version = trim(shell_exec("cd {$tmpSourceDir} && git rev-parse HEAD"));
        if (strlen($version) !== 40) {
            throw new RuntimeException("Invalid git version {$version}");
        }
        FileSystem::remove("{$tmpSourceDir}/.git");
    } else {
        throw new RuntimeException('Unsupported now');
    }
    if (file_exists($targetDir)) {
        try {
            FileSystem::move($targetDir, $tmpBackupDir);
        } catch (FileSystem\IOException $exception) {
            throw new RuntimeException("Backup from '{$targetDir}' to '{$tmpBackupDir}' failed: {$exception->getMessage()}");
        }
    }
    if (count($requires) > 0) {
        if (!mkdir($targetDir, 0755, true)) {
            throw new RuntimeException("Unable to create target source dir '{$targetDir}'");
        }
        foreach ($requires as $require) {
            $tmpSourceFile = "{$tmpSourceDir}/{$require}";
            $targetFile = "{$targetDir}/{$require}";
            try {
                FileSystem::move($tmpSourceFile, $targetFile);
            } catch (FileSystem\IOException $exception) {
                throw new RuntimeException("Update dep files from '{$tmpSourceFile}' to {$targetFile} failed: {$exception->getMessage()}");
            }
        }
    } else {
        $targetParentDir = dirname($targetDir);
        if (!is_dir($targetParentDir)) {
            if (!mkdir($targetParentDir, 0755, true)) {
                throw new RuntimeException("Unable to create target parent dir '{$targetParentDir}'");
            }
        }
        try {
            FileSystem::move($tmpSourceDir, $targetDir);
        } catch (FileSystem\IOException $exception) {
            throw new RuntimeException("Update dep files from '{$tmpSourceDir}' to {$targetDir} failed: {$exception->getMessage()}");
        }
    }
    shell_exec("cd {$targetDir} && git add --ignore-errors -A -- .");

    return $version;
};

$syncFromGit = static function (string $orgName, string $repoName, string $sourceDir, string $targetDir, array $requires = []) use ($sync): array {
    $fullName = "{$orgName}/{$repoName}";
    log("Sync {$fullName}...");
    $url = null;
    for ($baseLevel = 1; $baseLevel < 10; $baseLevel++) {
        $path = dirname(__DIR__, $baseLevel);
        if (file_exists("{$path}/.git")) {
            for ($level = $baseLevel + 1; $level < $baseLevel + 3; $level++) {
                $path = dirname(__DIR__, $level) . '/' . $repoName;
                if (file_exists("{$path}/.git")) {
                    $url = "file://{$path}";
                    break;
                }
            }
            break;
        }
    }
    if ($url === null) {
        $url = "git@github.com:{$fullName}.git";
    }
    $version = $sync($repoName, $url, $sourceDir, $targetDir, $requires);
    ok("Sync {$fullName} to {$version}");
    return [$repoName => "* {$fullName}@{$version}\n"];
};

try {
    $deps = [];
    $deps += $syncFromGit(
        orgName: 'libcat',
        repoName: 'libcat',
        sourceDir: 'libcat',
        targetDir: "{$workSpace}/libcat",
        requires: ['include', 'src', 'deps', 'tools', 'clean.sh', 'LICENSE']
    );
} catch (Exception $exception) {
    error($exception->getMessage());
}

$depNames = implode(', ', array_keys($deps));
$depsInfo = implode("\n", $deps);
notice('You can put following content to git commit message:');
log("Sync deps: {$depNames}\n\n{$depsInfo}");
