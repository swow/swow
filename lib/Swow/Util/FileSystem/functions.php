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

namespace Swow\Util\FileSystem;

function scan(string $dir, callable $filter = null): array
{
    $files = \array_filter(\scandir($dir), function (string $file) { return $file[0] !== '.'; });
    \array_walk($files, function (&$file) use ($dir) { $file = "{$dir}/{$file}"; });

    return \array_values($filter ? \array_filter($files, $filter) : $files);
}

function remove(string $path): void
{
    try {
        if (!is_dir($path) || is_link($path)) {
            \unlink($path);
        } else {
            $it = new \RecursiveDirectoryIterator($path, \RecursiveDirectoryIterator::SKIP_DOTS);
            $files = new \RecursiveIteratorIterator($it, \RecursiveIteratorIterator::CHILD_FIRST);
            /** @var \SplFileInfo $file */
            foreach ($files as $file) {
                remove($file->getRealPath());
            }
            \rmdir($path);
        }
    } catch (\Exception $exception) {
        // We don not want individual failures to lead to overall failures
        trigger_error($exception->getMessage(), E_USER_WARNING);
    }
}
