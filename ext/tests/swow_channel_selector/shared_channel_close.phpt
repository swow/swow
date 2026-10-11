--TEST--
swow_selector: closing a shared channel cleans each selector without disturbing other waiters
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

foreach ([0, 1] as $capacity) {
    foreach ([false, true] as $push) {
        for ($position = 0; $position < 3; $position++) {
            $shared = new Channel($capacity);
            if ($capacity !== 0 && $push) {
                $shared->push('occupied');
            }
            $channels = [];
            $workers = [];
            $failures = [];
            $references = [];
            try {
                for ($number = 0; $number < TEST_MAX_CONCURRENCY_LOW; $number++) {
                    $selector = new Selector();
                    $channels[$number] = [new Channel($capacity), new Channel($capacity), new Channel($capacity)];
                    $channels[$number][$position] = $shared;
                    foreach ($channels[$number] as $index => $channel) {
                        if ($push) {
                            if ($capacity !== 0 && $index !== $position) {
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
                    $workers[] = Coroutine::run(static function () use ($selector, $shared, $number, &$failures): void {
                        try {
                            $selector->commit(1000);
                            Assert::false('a shared closed channel unexpectedly completed selection');
                        } catch (SelectorException $exception) {
                            Assert::same($exception->getReturnValue(), $shared);
                            $failures[$number] = $exception->getCode();
                        }
                    });
                    Assert::true($workers[$number]->isAlive());
                    foreach ($channels[$number] as $channel) {
                        Assert::true($push ? $channel->hasProducers() : $channel->hasConsumers());
                    }
                }
                $shared->close();
                Assert::same(count($failures), count($workers));
                foreach ($workers as $number => $worker) {
                    Assert::false($worker->isAlive());
                    Assert::same($failures[$number], Errno::ECLOSED);
                    foreach ($channels[$number] as $channel) {
                        Assert::false($channel->hasProducers());
                        Assert::false($channel->hasConsumers());
                        if ($capacity !== 0 && $push && $channel !== $shared) {
                            Assert::same($channel->pop(0), 'occupied');
                        }
                    }
                }
                foreach ($references as $reference) {
                    Assert::null($reference->get());
                }
            } finally {
                foreach ($workers as $worker) {
                    if ($worker->isAlive()) {
                        $worker->kill();
                    }
                }
                foreach ($channels as $selection) {
                    foreach ($selection as $channel) {
                        if ($channel->isAvailable()) {
                            $channel->close();
                        }
                    }
                }
                if ($shared->isAvailable()) {
                    $shared->close();
                }
            }
            unset($selector, $channels, $channel, $selection, $workers, $worker, $shared);
            gc_collect_cycles();
        }
    }
}

echo "Done\n";
?>
--EXPECT--
Done
