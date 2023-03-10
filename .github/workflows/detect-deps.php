<?php
/**
 * This file is part of Swow
 *
 * @link    https://github.com/swow/swow
 * @contact twosee <twosee@php.net>
 *
 * For the full copyright and license information,
 * please view the LICENSE file that was distributed with this source code
 */

declare(strict_types=1);

if (PHP_OS_FAMILY !== 'Darwin') {
    echo 'This script is only for macOS for now';
    exit(1);
}
if (empty($argv[1])) {
    echo 'No argument provided';
    exit(1);
}
$getFullPath = static function (string $name): string {
    preg_match("/--ldflags[^\\[]+\\[.+-L(.+{$name}.*?)\\/?([^\\/]+)?\\/lib/", shell_exec('php-config'), $matches);
    $fullPath = sprintf('%s%s%s', $prefixPath = $matches[1], isset($matches[2]) ? '/' : '', $matches[2] ?? '');
    if (!is_dir($fullPath)) {
        $fullPath = sprintf('%s/%s', $prefixPath, trim(explode("\n", shell_exec("ls {$matches[1]}"))[0]));
    }
    return $fullPath;
};
if ($argv[1] === 'openssl') {
    // maybe: /usr/local/Cellar/openssl@1.1/1.1.1s
    echo $getFullPath('openssl');
} elseif ($argv[1] === 'curl') {
    // maybe: /usr/local/Cellar/curl/7.87.0
    echo $getFullPath('curl');
} elseif ($argv[1] === 'libpq') {
    // maybe: /usr/local/opt/libpq
    echo $getFullPath('libpq');
} else {
    echo "Unknown argument: {$argv[1]}";
    exit(1);
}
