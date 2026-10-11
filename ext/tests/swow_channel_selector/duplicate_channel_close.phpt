--TEST--
swow_selector: closing a channel registered repeatedly removes every producer and consumer
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
    foreach ($capacity === 0 ? range(0, 7) : [0, 7] as $mask) {
        $channel = new Channel($capacity);
        $selector = new Selector();
        $references = [];
        if ($capacity !== 0 && $mask !== 0) {
            $channel->push('occupied');
        }
        for ($index = 0; $index < 3; $index++) {
            if ($mask & (1 << $index)) {
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
                Assert::false('duplicate closed waits unexpectedly succeeded');
            } catch (SelectorException $exception) {
                $failure = $exception;
            }
        });
        try {
            Assert::true($worker->isAlive());
            Assert::same($channel->hasProducers(), $mask !== 0);
            Assert::same($channel->hasConsumers(), $mask !== 7);
            $channel->close();
            Assert::false($worker->isAlive());
            Assert::isInstanceOf($failure, SelectorException::class);
            Assert::same($failure->getCode(), Errno::ECLOSED);
            Assert::same($failure->getReturnValue(), $channel);
            Assert::false($channel->hasProducers());
            Assert::false($channel->hasConsumers());
            foreach ($references as $reference) {
                Assert::null($reference->get());
            }
        } finally {
            if ($worker->isAlive()) {
                $worker->kill();
            }
            if ($channel->isAvailable()) {
                $channel->close();
            }
        }
        unset($worker, $selector, $channel);
        gc_collect_cycles();
    }
}

echo "Done\n";
?>
--EXPECT--
Done
