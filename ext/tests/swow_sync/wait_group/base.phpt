--TEST--
swow_sync/wait_group: base
--SKIPIF--
<?php
require __DIR__ . '/../../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../../include/bootstrap.php';

use Swow\Sync\WaitGroup;
use Swow\Coroutine;

$wg = new WaitGroup();
$wg->add($n = 10);
while ($n--) {
    Coroutine::run(function () use ($wg, $n) {
        // mock doing some tasks consuming times
        pseudo_random_sleep();
        echo $n . PHP_LF;
        $wg->done();
    });
}
echo 'Start WG' . PHP_LF;
$wg->wait();
echo 'Done' . PHP_LF;

?>
--EXPECT--
Start WG
9
2
4
0
5
3
8
7
1
6
Done
