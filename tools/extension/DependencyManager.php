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

namespace Swow\Tools;

use RuntimeException;
use Swow\Util\FileSystem;
use function count;
use function date;
use function dirname;
use function file_exists;
use function is_dir;
use function is_writable;
use function mkdir;
use function parse_url;
use function strlen;
use function sys_get_temp_dir;
use function trim;

class DependencyManager
{
    public static function sync(string $name, string $url, string $sourceDir, string $targetDir, array $requires = []): string
    {
        if (PHP_OS_FAMILY === 'Windows') {
            throw new RuntimeException('Linux only');
        }
        $targetWorkspace = dirname($targetDir);
        if (file_exists($targetDir) && !`cd {$targetWorkspace} && git rev-parse HEAD`) {
            throw new RuntimeException("Unable to find git repo in {$targetDir}");
        }
        $sysTmpDir = static::getSysTmpDir();
        $tmpBackupDir = $sysTmpDir . '/swow_deps_backup_' . date('YmdHis');
        if (!is_dir($tmpBackupDir) && !mkdir($tmpBackupDir, 0755)) {
            throw new RuntimeException("Unable to create tmp backup dir '{$tmpBackupDir}'");
        }
        if (str_ends_with($url, '.git') || (parse_url($url)['scheme'] ?? '') === 'file') {
            $tmpSourceDir = $sysTmpDir . "/swow_deps/{$name}";
            if (is_dir($tmpSourceDir)) {
                FileSystem::remove($tmpSourceDir);
            }
            if (!mkdir($tmpSourceDir, 0755, true)) {
                throw new RuntimeException("Unable to create tmp source dir '{$tmpSourceDir}'");
            }
            defer($defer, function () use ($tmpSourceDir) {
                if (is_dir($tmpSourceDir)) {
                    FileSystem::remove($tmpSourceDir);
                }
            });
            `git clone --depth=1 {$url} {$tmpSourceDir}`;
            if (!is_dir("{$tmpSourceDir}/.git")) {
                throw new RuntimeException("Clone source files of {$name} failed");
            }
            $version = trim(`cd {$tmpSourceDir} && git rev-parse HEAD`);
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
        `cd {$targetDir} && git add --ignore-errors -A`;

        return $version;
    }

    protected static string $sysTmpDir;

    protected static function getSysTmpDir(): string
    {
        if (!isset(static::$sysTmpDir)) {
            if (is_writable('/tmp')) {
                static::$sysTmpDir = '/tmp';
            } else {
                static::$sysTmpDir = sys_get_temp_dir();
                notice('Sys temp dir is redirected to ' . static::$sysTmpDir);
            }
        }

        return static::$sysTmpDir;
    }
}
