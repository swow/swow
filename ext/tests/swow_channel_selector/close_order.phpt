--TEST--
swow_selector: closing any registered channel detaches all waits and releases unselected values
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
    for ($mask = 0; $mask < 8; $mask++) {
        for ($position = 0; $position < 3; $position++) {
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
            $worker = Coroutine::run(static function () use ($selector, &$failure): void {
                try {
                    $selector->commit(1000);
                    Assert::false('a closed channel unexpectedly completed selection');
                } catch (SelectorException $exception) {
                    $failure = $exception;
                }
            });
            $writer = null;
            try {
                Assert::true($worker->isAlive());
                // Coroutine::run() reaches the registered wait before returning to the closer.
                foreach ($channels as $index => $channel) {
                    Assert::true(($mask & (1 << $index)) ? $channel->hasProducers() : $channel->hasConsumers());
                }
                $channels[$position]->close();
                Assert::false($worker->isAlive());
                Assert::isInstanceOf($failure, SelectorException::class);
                Assert::same($failure->getCode(), Errno::ECLOSED);
                Assert::same($failure->getReturnValue(), $channels[$position]);
                Assert::same($selector->getLastEvent(), ($mask & (1 << $position)) ? Selector::EVENT_PUSH : Selector::EVENT_POP);
                foreach ($references as $reference) {
                    Assert::null($reference->get());
                }
                foreach ($channels as $index => $channel) {
                    Assert::false($channel->hasProducers());
                    Assert::false($channel->hasConsumers());
                    if (!$channel->isAvailable()) {
                        continue;
                    }
                    if ($capacity !== 0 && ($mask & (1 << $index))) {
                        Assert::same($channel->pop(0), 'occupied');
                    } else {
                        Assert::same($channel->getLength(), 0);
                    }
                    $writer = Coroutine::run(static function () use ($channel): void {
                        $channel->push('after close', 1000);
                    });
                    Assert::same($selector->pop($channel)->commit(1000), $channel);
                    Assert::same($selector->fetch(), 'after close');
                    Assert::false($writer->isAlive());
                    Assert::false($channel->hasProducers());
                    Assert::false($channel->hasConsumers());
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
