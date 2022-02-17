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

use Swow\Channel;
use Swow\Coroutine;

$channel = new Channel();
Coroutine::run(static function () use ($channel): void {
    while ($channel->pop()) {
        continue;
    }
    echo 'Over' . PHP_EOL;
});

$times = 1000 * 10000;
$use = microtime(true);
for ($n = $times / 2; $n--;) {
    $channel->push(true);
}
$use = microtime(true) - $use;
$channel->push(false);

$ns = $use * (1000 * 1000 * 1000) / $times;
$qps = $times * (1 / $use);

echo sprintf('Use %fs for %d times, %fns/t, qps=%f' . PHP_EOL, $use, $times, $ns, $qps);
