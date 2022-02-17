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
use Swow\Channel\CallbackSelector as Selector;
use Swow\Coroutine;

$channel1 = new Channel();
$channel2 = new Channel();
$channel3 = new Channel();

Coroutine::run(static function () use ($channel1): void {
    sleep(1);
    $channel1->push('one');
});
Coroutine::run(static function () use ($channel2): void {
    sleep(2);
    $channel2->push('two');
});
Coroutine::run(static function () use ($channel3): void {
    sleep(3);
    $channel3->push('three');
});

$s = new Selector();
for ($n = 3; $n--;) {
    $s->casePop($channel1, static function ($data): void {
        echo "Receive {$data} from channel-1\n";
    })->casePop($channel2, static function ($data): void {
        echo "Receive {$data} from channel-2\n";
    })->casePop($channel3, static function ($data): void {
        echo "Receive {$data} from channel-3\n";
    })->select();
}
