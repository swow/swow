#!/usr/bin/env php
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

(static function () use ($argv): void {
    $options = getopt('d:');
    $requiredExtensions = [];
    if (isset($options['d'])) {
        $commandLoadedExtensions = is_array($options['d']) ? $options['d'] : [$options['d']];
        foreach ($commandLoadedExtensions as $commandLoadedExtension) {
            if (str_starts_with($commandLoadedExtension, 'extension=')) {
                [, $commandLoadedExtension] = explode('=', $commandLoadedExtension);
                $requiredExtensions[] = $commandLoadedExtension;
            }
        }
    }

    $proc = proc_open(
        [PHP_BINARY, '-r', 'echo serialize(get_loaded_extensions());'],
        [0 => STDIN, 1 => ['pipe', 'w'], 2 => ['redirect', 1]],
        $pipes, null, null
    );
    $output = '';
    do {
        $output .= fread($pipes[1], 8192);
    } while (!feof($pipes[1]));
    // $exitCode = proc_get_status($proc)['exitcode'];
    proc_close($proc);
    // FIXME: Workaround: ignore exit code, it may be -1 on some platforms but it's ok
    // if ($exitCode !== 0) {
    //     throw new RuntimeException(sprintf('Failed to get loaded extensions with exit code %d and output "%s"', $exitCode, addcslashes($output, "\r\n")));
    // }
    $defaultLoadedExtensions = !empty($output) ? (array) @unserialize($output) : [];
    $defaultLoadedExtensions = array_map('strtolower', $defaultLoadedExtensions);

    $iniFiles = [];
    $mainIniFile = php_ini_loaded_file();
    if ($mainIniFile !== false) {
        $iniFiles[] = $mainIniFile;
    }
    $scannedFiles = php_ini_scanned_files();
    if ($scannedFiles !== false) {
        $scannedFiles = explode(',', $scannedFiles);
        foreach ($scannedFiles as $file) {
            $file = trim($file);
            if ($file !== '') {
                $iniFiles[] = $file;
            }
        }
    }
    $iniLoadedExtensions = [];
    foreach ($iniFiles as $iniFile) {
        $parsedIni = parse_ini_file($iniFile, true);
        $iniLoadedExtensions = [];
        array_walk_recursive($parsedIni, static function (mixed $value, mixed $key) use (&$iniLoadedExtensions): void {
            if ($key === 'extension') {
                $iniLoadedExtensions[] = basename((string) $value);
            }
        });
    }

    $unloadedExtensions = array_diff(
        $requiredExtensions,
        $defaultLoadedExtensions,
    );
    $unloadedExtensions = array_diff(
        $unloadedExtensions,
        $iniLoadedExtensions,
    );

    $args = array_slice($argv, 1);
    $args = array_filter($args, static function ($arg) {
        return !str_starts_with($arg, '-dextension=') && $arg !== '-d' && !str_starts_with($arg, 'extension=');
    });
    $unloadedExtensionsArgs = [];
    foreach ($unloadedExtensions as $unloadedExtension) {
        $unloadedExtensionsArgs[] = '-d';
        $unloadedExtensionsArgs[] = 'extension=' . $unloadedExtension;
    }
    $proc = proc_open(
        [PHP_BINARY, ...$unloadedExtensionsArgs, ...$args],
        [0 => STDIN, 1 => [PHP_OS_FAMILY === 'Windows' ? 'pipe' : 'pty', 'w'], 2 => STDERR],
        $pipes, null, null
    );
    do {
        echo @fread($pipes[1], 8192);
    } while (!feof($pipes[1]));
    /* FIXME: workaround for some platforms */
    if (function_exists('pcntl_waitpid')) {
        $status = proc_get_status($proc);
        if ($status['running']) {
            pcntl_waitpid($status['pid'], $wStatus);
            $exitCode = pcntl_wexitstatus($wStatus);
        } else {
            $exitCode = $status['exitcode'];
        }
    } else {
        while (true) {
            $status = proc_get_status($proc);
            if (!$status['running']) {
                $exitCode = $status['exitcode'];
                break;
            }
            usleep(1000);
        }
    }
    proc_close($proc);
    exit($exitCode);
})();
