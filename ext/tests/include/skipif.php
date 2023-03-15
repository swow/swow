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

if (!extension_loaded('swow')) {
    skip('Swow extension is required');
}

function skip(string $reason): void
{
    exit("skip {$reason}");
}

function skip_if(bool $condition, string $reason): void
{
    if ($condition) {
        skip($reason);
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
    if (((bool) ini_get($name)) === $value) {
        $value = $value ? 'enable' : 'disable';
        skip("{$name} is {$value}");
    }
}

function skip_if_constant_not_defined(string $constant_name): void
{
    skip_if(!defined($constant_name), "{$constant_name} is not defined");
}

function skip_if_env_not_true(string $env_name): void
{
    skip_if(!in_array(strtolower(getenv($env_name)?:""), ["1", "true", "on", "yes"]), "{$env_name} is not true");
}

function skip_if_function_not_exist(string $function_name): void
{
    skip_if(!function_exists($function_name), "{$function_name} not exist");
}

function skip_if_class_not_exist(string $class_name): void
{
    skip_if(!class_exists($class_name, false), "{$class_name} not exist");
}

function skip_if_extension_not_exist(string $extension_name): void
{
    skip_if(!extension_loaded($extension_name), "{$extension_name} not exist");
}

function skip_if_file_not_exist(string $filename): void
{
    skip_if(!file_exists($filename), "file {$filename} is not exist");
}

function skip_linux_only(string $reason = 'Linux only'): void
{
    skip_if(PHP_OS_FAMILY !== 'Linux', $reason);
}

function skip_if_win(string $reason = 'Not support on Windows'): void
{
    skip_if(PHP_OS_FAMILY === 'Windows', $reason);
}

function skip_if_darwin(string $reason = 'Not support on Darwin'): void
{
    skip_if(PHP_OS_FAMILY === 'Darwin', $reason);
}

function skip_if_musl_libc(string $reason = 'Not support when use musl libc'): void
{
    skip_if(!empty(shell_exec('ldd 2>&1 | grep -i musl')), $reason);
}

function skip_if_in_valgrind(string $reason = 'Valgrind is too slow'): void
{
    skip_if(getenv('USE_ZEND_ALLOC') === '0', $reason);
}

function skip_if_in_travis(string $reason = 'Not support in travis'): void
{
    skip_if(file_exists('/.travisenv'), $reason);
}

function skip_if_in_docker(string $reason = 'Not support in docker'): void
{
    skip_if(file_exists('/.dockerenv'), $reason);
}

function skip_unsupported(string $reason = 'This test cannot continue to work for some implementation reasons'): void
{
    skip($reason);
}

function skip_if_c_function_not_exist(string $def, ?string $lib = null): void
{
    skip_if_extension_not_exist('ffi');

    try {
        if ($lib) {
            FFI::cdef($def, $lib);
        } else {
            FFI::cdef($def);
        }
    } catch (Throwable) {
        skip('"' . $def . '" not usable');
    }
}

function skip_if_cannot_make_subprocess(): void
{
    skip_if((!is_callable('shell_exec')) || (!is_callable('popen')), 'shell_exec is not callable');
    $loaded_modules = shell_exec(PHP_BINARY . ' -m');
    if (!str_contains($loaded_modules, Swow::class)) {
        $loaded_modules = shell_exec(PHP_BINARY . ' -dextension=swow --ri swow');
        if (
            !str_contains($loaded_modules, Swow::class) ||
            str_contains($loaded_modules, 'Warning')
        ) {
            skip('Swow is not present in TEST_PHP_EXECUTABLE and cannot load it via -dextension=swow');
        }
    }
}

function skip_if_max_open_files_less_than(int $number): void
{
    $n = null;
    if (PHP_OS_FAMILY === 'Windows') {
        $n = PHP_INT_MAX;
    } else {
        if (function_exists('posix_getrlimit')) {
            $n = posix_getrlimit()['soft openfiles'] ?? null;
        }
        $n ??= (int) shell_exec('ulmit -n');
    }
    if ($n < $number) {
        skip("Max open files should be greater than or equal to {$number}, but it is {$n} now");
    }
}

function skip_if_offline(): void
{
    skip_if(getenv('OFFLINE'), 'Internet connection required');
}
