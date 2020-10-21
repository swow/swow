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
        echo $n . PHP_LF;
        $wg->done();
    });
}
$wg->wait();
echo 'Done' . PHP_LF;

?>
--EXPECT--
9
8
7
6
5
4
3
2
1
0
Done
