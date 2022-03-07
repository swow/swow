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
    $options = getopt('h', ['help'], $restIndex);
    $argv = array_slice($argv, $restIndex);
    $filename = $argv[0] ?? '';
    if (isset($options['h']) || isset($options['help']) || empty($filename)) {
        $basename = basename(__FILE__);
        exit("Usage: php {$basename} <file>");
    }
    $fp = @fopen($filename, 'rb');
    if ($fp === false) {
        throw new RuntimeException(error_get_last()['message']);
    }
    $lines = [];
    while (($line = fgets($fp)) !== false) {
        $lines[] = rtrim($line);
    }
    $emptyLines = 0;
    for ($n = count($lines) - 1; $n >= 0; $n--) {
        if (empty($lines[$n])) {
            $emptyLines++;
        } else {
            break;
        }
    }
    $lines = array_slice($lines, 0, count($lines) - $emptyLines);
    $lines[] = ''; // the last empty line
    fclose($fp);

    $content = implode("\n", $lines);
    if (!@file_put_contents($filename, $content)) {
        throw new RuntimeException(error_get_last()['message']);
    }
})();
