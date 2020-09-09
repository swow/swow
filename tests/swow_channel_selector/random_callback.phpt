--TEST--
swow_channel_selector: random (callback version)
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Channel;
use Swow\Channel\CallbackSelector as Selector;
use Swow\Coroutine;

$s = new Selector();

$channels = [];
for ($l = TEST_MAX_LENGTH; $l--;) {
    $channels[] = new Channel();
}

for ($n = TEST_MAX_REQUESTS; $n--;) {
    $targetChannel = $channels[array_rand($channels)];
    $opcode = mt_rand(0, 1) ? Channel::OPCODE_PUSH : Channel::OPCODE_POP;
    $randomBytes = getRandomBytes();
    $coroutine = Coroutine::run(function () use ($targetChannel, $opcode, $randomBytes) {
        if ($opcode === Channel::OPCODE_POP) {
            $targetChannel->push($randomBytes);
        } else {
            Assert::same($targetChannel->pop(), $randomBytes);
        }
    });
    foreach ($channels as $channel) {
        if ($opcode === Channel::OPCODE_PUSH) {
            $s->casePush($channel, $randomBytes, function () use ($coroutine) {
                Assert::false($coroutine->isAvailable());
            });
        } else {
            $s->casePop($channel, function ($data) use ($randomBytes) {
                Assert::same($data, $randomBytes);
            });
        }
    }
    $s->select();
}

echo 'Done' . PHP_LF;

?>
--EXPECT--
Done
