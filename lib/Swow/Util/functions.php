<?php
/**
 * This file is part of Swow
 *
 * @link     https://github.com/swow/swow
 * @contact  twosee <twose@qq.com>
 *
 * For the full copyright and license information,
 * please view the LICENSE file that was distributed with this source code
 */

declare(strict_types=1);

namespace Swow\Util;

function scanDir(string $dir, callable $filter = null): array
{
    $files = array_filter(\scandir($dir), function (string $file) { return $file[0] !== '.'; });
    array_walk($files, function (&$file) use ($dir) { $file = "{$dir}/{$file}"; });

    return array_values($filter ? array_filter($files, $filter) : $files);
}
