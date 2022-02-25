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

require __DIR__ . '/../autoload.php';

use Swow\Channel;
use Swow\Coroutine;
use function Swow\Debug\registerDeadlockDetectorHandler;
use function Swow\Sync\waitAll;

registerDeadlockDetectorHandler();

$channel = new Channel();

// producers
for ($n = 0; $n < 10; $n++) {
    Coroutine::run(static function () use ($channel, $n): void {
        while (true) {
            usleep(mt_rand(1000, 10000));
            $random = base64_encode(random_bytes(8));
            echo "\$channel{$n}->push({$random})\n";
            $channel->push($random);
            if (mt_rand(0, 4) === 0) {
                break;
            }
        }
    });
}

// consumers
for ($n = 0; $n < 10; $n++) {
    Coroutine::run(static function () use ($channel, $n): void {
        while (true) {
            echo "\$channel{$n}->pop() = {$channel->pop()}\n";
        }
    });
}

/* The producer will exit at a random time,
 * and the consumer will be consuming all the time,
 * so deadlock will always occur */
waitAll();
