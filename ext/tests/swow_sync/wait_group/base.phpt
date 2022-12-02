--TEST--
swow_sync/wait_group: base
--SKIPIF--
<?php
require __DIR__ . '/../../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../../include/bootstrap.php';

use Swow\Coroutine;
use Swow\Sync\WaitGroup;

$wg = new WaitGroup();
$wg->add($n = 10);
while ($n--) {
    Coroutine::run(static function () use ($wg, $n): void {
        // mock doing some tasks consuming times
        pseudo_random_sleep();
        echo $n . "\n";
        $wg->done();
    });
}
echo "Start WG\n";
$wg->wait();
echo "Done\n";

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
