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

if (!extension_loaded('swow')) {
    skip('Swow extension is required');
}

function skip(string $reason, bool $condition = true): void
{
    if ($condition) {
        exit("skip {$reason}");
    }
}

function skip_if_php_version_lower_than($require_version = '7.0'): void
{
    if (version_compare(PHP_VERSION, $require_version, '<')) {
        skip('need php version >= ' . $require_version);
    }
}

function skip_if_php_version_between($a, $b): void
{
    if (version_compare(PHP_VERSION, $a, '>=') && version_compare(PHP_VERSION, $b, '<=')) {
        skip("unsupported php version between {$a} and {$b}");
    }
}

function skip_if_ini_bool_equal_to(string $name, bool $value): void
{
    if (((bool)ini_get($name)) === $value) {
        $value = $value ? 'enable' : 'disable';
        skip("{$name} is {$value}");
    }
}

function skip_if_constant_not_defined(string $constant_name): void
{
    skip("{$constant_name} is not defined", !defined($constant_name));
}

function skip_if_function_not_exist(string $function_name): void
{
    skip("{$function_name} not exist", !function_exists($function_name));
}

function skip_if_class_not_exist(string $class_name): void
{
    skip("{$class_name} not exist", !class_exists($class_name, false));
}

function skip_if_extension_not_exist(string $extension_name): void
{
    skip("{$extension_name} not exist", !extension_loaded($extension_name));
}

function skip_if_file_not_exist(string $filename): void
{
    skip("file {$filename} is not exist", !file_exists($filename));
}

function skip_linux_only(): void
{
    skip('Linux only', PHP_OS_FAMILY !== 'Linux');
}

function skip_if_win(): void
{
    skip('Not support on Windows', PHP_OS_FAMILY === 'Windows');
}

function skip_if_darwin(): void
{
    skip('Not support on Darwin', PHP_OS_FAMILY === 'Darwin');
}

function skip_if_musl_libc(): void
{
    skip('Not support when use musl libc', !empty(`ldd 2>&1 | grep -i musl`));
}

function skip_if_in_valgrind(string $reason = 'valgrind is too slow'): void
{
    skip($reason, getenv('USE_ZEND_ALLOC') === '0');
}

function skip_if_in_travis(string $reason = 'not support in travis'): void
{
    skip($reason, file_exists('/.travisenv'));
}

function skip_if_in_docker(string $reason = 'not support in docker'): void
{
    skip($reason, file_exists('/.dockerenv'));
}

function skip_unsupported(string $message = ''): void
{
    skip($message ?: 'the test cannot continue to work for some implementation reasons');
}
