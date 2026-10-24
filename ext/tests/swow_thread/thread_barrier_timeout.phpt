--TEST--
swow_thread_barrier: a timed out waiter never wraps the waiting count
--SKIPIF--
<?php
if (!PHP_ZTS) {
    exit('skip ZTS only');
}
require __DIR__ . '/../include/skipif.php';
?>
--INI--
swow.closure_serializer=1
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Sync\ThreadBarrier;
use Swow\Thread;

$barrier = new ThreadBarrier(2);

for ($i = 0; $i < 50; $i++) {
    $thread = Thread::run(static function (ThreadBarrier $barrier): bool {
        return $barrier->wait(1);
    }, $barrier);
    usleep(random_int(500, 3000));
    // the round trips when the thread is still waiting, and ends with a timeout
    // otherwise
    $barrier->wait(1);
    $thread->wait();

    // a trip clears the count and a timeout leaves it, so a negative count means
    // that one of the two ran twice
    Assert::same(0, $barrier->getWaiting());
}

// a wrapped count would make this round wait for three participants
$thread = Thread::run(static function (ThreadBarrier $barrier): bool {
    return $barrier->wait(5000);
}, $barrier);
usleep(20000);
Assert::true($barrier->wait(5000));
Assert::true($thread->wait());
Assert::same(0, $barrier->getWaiting());

echo "done\n";
?>
--EXPECT--
done
