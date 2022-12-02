--TEST--
swow_channel: pop then push
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
        Coroutine::run(static function () use ($channel, $sleep): void {
            if ($sleep) {
                sleep(0);
            }
            while ($channel->pop()) {
                continue;
            }
        });
        for ($n = TEST_MAX_LOOPS; $n--;) {
            $channel->push(true);
        }
        $channel->push(false);
    }
}
echo "Done\n";

?>
--EXPECT--
Done
