--TEST--
swow_selector: random
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Channel;
use Swow\Selector;
use Swow\Coroutine;

$s = new Selector();

$channels = [];
for ($l = TEST_MAX_LENGTH; $l--;) {
    $channels[] = new Channel();
}

for ($n = TEST_MAX_REQUESTS; $n--;) {
    $targetChannel = $channels[array_rand($channels)];
    $opcode = mt_rand(0, 1) ? Selector::EVENT_PUSH : Selector::EVENT_POP;
    $randomBytes = getRandomBytes();
    Coroutine::run(function () use ($targetChannel, $opcode, $randomBytes) {
        if ($opcode === Selector::EVENT_POP) {
            $targetChannel->push($randomBytes);
        } else {
            Assert::same($targetChannel->pop(), $randomBytes);
        }
    });
    foreach ($channels as $channel) {
        if ($opcode === Selector::EVENT_PUSH) {
            $s->push($channel, $randomBytes);
        } else {
            $s->pop($channel);
        }
    }
    $channel = $s->commit();
    Assert::same($channel, $targetChannel);
    Assert::same($s->getLastEvent(), $opcode);
    if ($opcode === Selector::EVENT_POP) {
        Assert::same($s->fetch(), $randomBytes);
    }
}

echo "Done\n";

?>
--EXPECT--
Done
