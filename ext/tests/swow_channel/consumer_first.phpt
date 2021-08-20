--TEST--
swow_channel: consumer first
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Channel;
use Swow\Coroutine;

$channel = new Channel();
$n = 10;
Coroutine::run(function () use ($channel, $n) {
    echo 'Pop start' . PHP_LF;
    for ($i = 1; $i <= $n; $i++) {
        $ret = $channel->pop();
        echo "Pop #{$i} then return " . var_export($ret, true) . PHP_LF;
    }
});
Coroutine::run(function () use ($channel, $n) {
    echo 'Push start' . PHP_LF;
    for ($i = 1; $i <= $n; $i++) {
        $channel->push($i * $i);
        echo "Push#{$i} done" . PHP_LF;
    }
});

echo 'Done' . PHP_LF;

?>
--EXPECT--
Pop start
Push start
Pop #1 then return 1
Push#1 done
Pop #2 then return 4
Push#2 done
Pop #3 then return 9
Push#3 done
Pop #4 then return 16
Push#4 done
Pop #5 then return 25
Push#5 done
Pop #6 then return 36
Push#6 done
Pop #7 then return 49
Push#7 done
Pop #8 then return 64
Push#8 done
Pop #9 then return 81
Push#9 done
Pop #10 then return 100
Push#10 done
Done
