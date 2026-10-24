<?php

declare(strict_types=1);

namespace Swow\Examples\Gemm\Support;

use function file_get_contents;
use function ini_get;
use function php_uname;
use function preg_match;
use function trim;

final class System
{
    public static function cpuName(): string
    {
        $info = @file_get_contents('/proc/cpuinfo');
        if ($info !== false && preg_match('/^model name\s*:\s*(.+)$/m', $info, $matches) === 1) {
            return trim($matches[1]);
        }

        return php_uname('m');
    }

    public static function jit(): string
    {
        $jit = ini_get('opcache.jit');

        return $jit === false || $jit === '' ? 'off' : $jit;
    }
}
