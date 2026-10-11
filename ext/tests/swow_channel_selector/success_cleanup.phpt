--TEST--
swow_selector: successful mixed selection consumes one event and detaches every other wait
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Channel;
use Swow\Coroutine;
use Swow\Selector;

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
                    $value->index = $index;
                    $references[$index] = WeakReference::create($value);
                    $selector->push($channel, $value);
                    unset($value);
                } else {
                    $selector->pop($channel);
                }
            }
            $selected = null;
            $received = null;
            $worker = Coroutine::run(static function () use ($selector, &$selected, &$received): void {
                $selected = $selector->commit(1000);
                if ($selector->getLastEvent() === Selector::EVENT_POP) {
                    $received = $selector->fetch();
                }
            });
            $value = null;
            try {
                Assert::true($worker->isAlive());
                foreach ($channels as $index => $channel) {
                    Assert::true(($mask & (1 << $index)) ? $channel->hasProducers() : $channel->hasConsumers());
                }
                if ($mask & (1 << $position)) {
                    $value = $channels[$position]->pop(0);
                    if ($capacity !== 0) {
                        Assert::same($value, 'occupied');
                        $value = $channels[$position]->pop(0);
                    }
                    Assert::isInstanceOf($value, stdClass::class);
                    Assert::same($value->index, $position);
                    Assert::same($value, $references[$position]->get());
                } else {
                    $channels[$position]->push('selected', 0);
                    Assert::same($received, 'selected');
                }
                Assert::false($worker->isAlive());
                Assert::same($selected, $channels[$position]);
                Assert::same($selector->getLastEvent(), ($mask & (1 << $position)) ? Selector::EVENT_PUSH : Selector::EVENT_POP);
                foreach ($channels as $index => $channel) {
                    Assert::false($channel->hasProducers());
                    Assert::false($channel->hasConsumers());
                    if ($index !== $position && $capacity !== 0 && ($mask & (1 << $index))) {
                        Assert::same($channel->pop(0), 'occupied');
                    } else {
                        Assert::same($channel->getLength(), 0);
                    }
                }
                foreach ($references as $index => $reference) {
                    if ($index !== $position) {
                        Assert::null($reference->get());
                    }
                }
                unset($value);
                foreach ($references as $reference) {
                    Assert::null($reference->get());
                }
            } finally {
                if ($worker->isAlive()) {
                    $worker->kill();
                }
                foreach ($channels as $channel) {
                    if ($channel->isAvailable()) {
                        $channel->close();
                    }
                }
            }
            unset($selector, $channels, $channel, $worker, $selected);
            gc_collect_cycles();
        }
    }
}

echo "Done\n";
?>
--EXPECT--
Done
