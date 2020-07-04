--TEST--
swow_channel: push then pop
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Channel;
use Swow\Coroutine;

foreach ([false, true] as $sleep) {
    foreach ([0, 1, 100] as $c) {
        $channel = new Channel($c);
        Coroutine::run(function () use ($channel, $sleep) {
            if ($sleep) {
                sleep(0);
            }
            for ($n = TEST_MAX_LOOPS; $n--;) {
                $channel->push(true);
            }
            $channel->push(false);
        });
        while ($channel->pop()) {
            continue;
        }
    }
}
echo 'Done' . PHP_LF;

?>
--EXPECT--
Done
