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

$use = microtime(true);
$times = 1000 * 10000;
for ($n = $times; $n--;) {
    sleep(0);
}
echo 'Over' . PHP_EOL;
$use = microtime(true) - $use;
$ns = $use * (1000 * 1000 * 1000) / $times;
$qps = $times * (1 / $use);

echo sprintf('Use %fs for %d times, %fns/t, qps=%f', $use, $times, $ns, $qps);
