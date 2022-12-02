--TEST--
swow_sync/wait_reference: base
--SKIPIF--
<?php
require __DIR__ . '/../../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../../include/bootstrap.php';

use Swow\Coroutine;
use Swow\Sync\WaitReference;

$wr = new WaitReference();
// wait will return immediately when there's no more reference counts
// and unset $wr
WaitReference::wait($wr);

Assert::null($wr);

// common WaitReference usage: create and wait it on same scope
$wr = new WaitReference();
for ($n = 9; $n >= 0; $n--) {
    Coroutine::run(static function () use ($wr, $n): void {
        // mock doing some tasks consuming times
        pseudo_random_sleep();
        echo $n . "\n";
    });
}
echo "Start WR 1\n";
WaitReference::wait($wr);
echo "Done 1\n";

// uncommon WaitReference usage: create and wait it on different scope
$wr = new WaitReference();
for ($n = 9; $n >= 0; $n--) {
    Coroutine::run(static function () use ($wr, $n): void {
        // mock a time-consuming action
        pseudo_random_sleep();
        echo $n . "\n";
    });
}
// wait it on another coroutine
// note: DO NOT "use ($wr)" here, the Callable will add reference in main scope
//       pass $wr via arguments instead.
Coroutine::run(static function ($wr): void {
    echo "Start WR 2\n";
    WaitReference::wait($wr);
    echo "Done 2\n";
}, $wr);
// decline reference in main coroutine
unset($wr);
echo "Done main 2\n";

while (Coroutine::count() !== 1) {
    sleep(0);
}

?>
--EXPECT--
Start WR 1
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
Done 1
Start WR 2
Done main 2
0
4
1
3
8
7
9
2
6
5
Done 2
