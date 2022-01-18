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

/* @noinspection PhpUnusedLocalVariableInspection */

declare(strict_types=1);

$bigString = str_repeat('A', 10 * 1024 * 1024);

(static function () use ($bigString): void {
    $buffer = fopen('php://memory', 'r+');
    $m = memory_get_usage();
    fwrite($buffer, $bigString);
    fseek($buffer, 0);
    $dupString = stream_get_contents($buffer);
    $m = memory_get_usage() - $m;
    var_dump($m);
})();

(static function () use ($bigString): void {
    $buffer = new Swow\Buffer(0);
    $m = memory_get_usage();
    $buffer->write($bigString);
    $dupString = $buffer->toString();
    $m = memory_get_usage() - $m;
    var_dump($m);
})();
