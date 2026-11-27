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

/**
 * Runs the probe of a test in a PHP process without Swow, so a hooked function
 * can be compared with the native one on the same machine.
 *
 * A test calls NativeBaseline::run(__FILE__, ...) in the Swow process; the child
 * runs the same file with --native, where the test calls NativeBaseline::emit()
 * with its probe result and exits.
 */
final class NativeBaseline
{
    private const NATIVE_FLAG = '--native';

    public static function isNative(): bool
    {
        return ($GLOBALS['argv'][1] ?? null) === self::NATIVE_FLAG;
    }

    public static function emit(mixed $result): never
    {
        Assert::false(extension_loaded('swow'), 'The native baseline must not load Swow');
        echo json_encode($result, JSON_THROW_ON_ERROR | JSON_PARTIAL_OUTPUT_ON_ERROR);
        exit(0);
    }

    /**
     * @param list<string> $extensions extensions the probe needs, loaded when they are shared modules
     * @param array<string, string> $ini
     */
    public static function run(string $file, array $extensions = [], array $ini = []): mixed
    {
        Assert::true(extension_loaded('swow'));
        // A clean ini keeps the installed Swow module out of the native process.
        $arguments = [PHP_BINARY, '-n', '-d', 'extension_dir=' . ini_get('extension_dir')];
        foreach ($ini as $name => $value) {
            $arguments[] = '-d';
            $arguments[] = "{$name}={$value}";
        }
        $builtin = json_decode(self::execute([...$arguments, '-r', 'echo json_encode(array_map("strtolower", get_loaded_extensions()));']), true, flags: JSON_THROW_ON_ERROR);
        foreach ($extensions as $extension) {
            if (!in_array(strtolower($extension), $builtin, true)) {
                $arguments[] = '-d';
                $arguments[] = 'extension=' . $extension;
            }
        }
        return json_decode(self::execute([...$arguments, $file, self::NATIVE_FLAG]), true, flags: JSON_THROW_ON_ERROR);
    }

    /** @param list<string> $arguments */
    private static function execute(array $arguments): string
    {
        $process = proc_open($arguments, [0 => ['pipe', 'r'], 1 => ['pipe', 'w'], 2 => ['pipe', 'w']], $pipes);
        Assert::true(is_resource($process));
        fclose($pipes[0]);
        $output = stream_get_contents($pipes[1]);
        $errors = stream_get_contents($pipes[2]);
        fclose($pipes[1]);
        fclose($pipes[2]);
        Assert::same(proc_close($process), 0, 'native baseline failed: ' . $output . $errors);
        return $output;
    }
}
