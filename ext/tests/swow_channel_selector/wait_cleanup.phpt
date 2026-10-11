--TEST--
swow_selector: timeout and coroutine interruption detach all waits and release push values
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Channel;
use Swow\Coroutine;
use Swow\Errno;
use Swow\Selector;
use Swow\SelectorException;

use function Swow\Sync\waitAll;

foreach ([0, 1] as $capacity) {
    for ($mask = 0; $mask < 8; $mask++) {
        foreach (['timeout-zero', 'timeout', 'resume', 'throw', 'kill'] as $operation) {
            $channels = [new Channel($capacity), new Channel($capacity), new Channel($capacity)];
            $selector = new Selector();
            $references = [];
            foreach ($channels as $index => $channel) {
                if ($mask & (1 << $index)) {
                    if ($capacity !== 0) {
                        $channel->push('occupied');
                    }
                    $value = new stdClass();
                    $references[] = WeakReference::create($value);
                    $selector->push($channel, $value);
                    unset($value);
                } else {
                    $selector->pop($channel);
                }
            }
            $failure = null;
            $injected = new RuntimeException('interrupt selection');
            $timeout = $operation === 'timeout-zero' ? 0 : ($operation === 'timeout' ? 1 : 1000);
            $worker = Coroutine::run(static function () use ($selector, $timeout, &$failure): void {
                try {
                    $selector->commit($timeout);
                    Assert::false('an interrupted wait unexpectedly completed selection');
                } catch (SelectorException $exception) {
                    $failure = $exception;
                }
            });
            $writer = null;
            try {
                if ($timeout !== 0) {
                    Assert::true($worker->isAlive());
                    foreach ($channels as $index => $channel) {
                        Assert::true(($mask & (1 << $index)) ? $channel->hasProducers() : $channel->hasConsumers());
                    }
                }
                if ($operation === 'resume') {
                    $worker->resume();
                } elseif ($operation === 'throw') {
                    $worker->throw($injected);
                } elseif ($operation === 'kill') {
                    $worker->kill();
                }
                // Only the timed wait can still be running; let its timer finish before inspecting cleanup.
                if ($worker->isAlive()) {
                    waitAll();
                }
                if ($operation === 'kill') {
                    Assert::null($failure);
                } else {
                    Assert::isInstanceOf($failure, SelectorException::class);
                    Assert::same($failure->getCode(), str_starts_with($operation, 'timeout') ? Errno::ETIMEDOUT : Errno::ECANCELED);
                    Assert::null($failure->getReturnValue());
                    if ($operation === 'throw') {
                        Assert::same($failure->getPrevious(), $injected);
                    }
                }
                Assert::false($worker->isAlive());
                foreach ($references as $reference) {
                    Assert::null($reference->get());
                }
                foreach ($channels as $index => $channel) {
                    Assert::false($channel->hasProducers());
                    Assert::false($channel->hasConsumers());
                    if ($capacity !== 0 && ($mask & (1 << $index))) {
                        Assert::same($channel->pop(0), 'occupied');
                    }
                    $writer = Coroutine::run(static function () use ($channel): void {
                        $channel->push('after interruption', 1000);
                    });
                    Assert::same($selector->pop($channel)->commit(1000), $channel);
                    Assert::same($selector->fetch(), 'after interruption');
                    Assert::false($writer->isAlive());
                }
            } finally {
                foreach ([$worker, $writer] as $coroutine) {
                    if ($coroutine !== null && $coroutine->isAlive()) {
                        $coroutine->kill();
                    }
                }
                foreach ($channels as $channel) {
                    if ($channel->isAvailable()) {
                        $channel->close();
                    }
                }
            }
            unset($selector, $channels, $channel, $worker, $writer);
            gc_collect_cycles();
        }
    }
}

echo "Done\n";
?>
--EXPECT--
Done
