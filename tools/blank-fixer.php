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
    $pattern = $argv[0] ?? '';
    if (isset($options['h']) || isset($options['help']) || empty($pattern)) {
        $basename = basename(__FILE__);
        exit("Usage: php {$basename} <pattern>\n");
    }
    // 返回多个
    $files = glob($pattern, GLOB_BRACE);
    if ($files === false) {
        throw new RuntimeException('Failed to glob files');
    }
    if (empty($files)) {
        exit("No files found\n");
    }
    $files = array_map('realpath', $files);
    echo "Are you sure to fix the blank lines in the following files?\n";
    foreach ($files as $file) {
        echo "    {$file}\n";
    }
    echo "Type 'Y' to continue: ";
    $line = rtrim(fgets(STDIN));
    if ($line !== 'Y') {
        exit("Aborted\n");
    }
    foreach ($files as $filename) {
        $fp = @fopen($filename, 'rb');
        if ($fp === false) {
            throw new RuntimeException(error_get_last()['message']);
        }
        $lines = [];
        while (($line = fgets($fp)) !== false) {
            $line = rtrim($line);
            $line = str_replace("\t", '    ', $line);
            $lines[] = $line;
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

        $contents = implode("\n", $lines);
        if (!@file_put_contents($filename, $contents)) {
            throw new RuntimeException(error_get_last()['message']);
        }
    }
})();
