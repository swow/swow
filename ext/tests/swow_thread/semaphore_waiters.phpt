--TEST--
swow_semaphore: release wakes multiple cross-thread waiters
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

$semaphore = new Swow\Semaphore(0);
$threads = [];

for ($i = 0; $i < 2; $i++) {
    $thread = new Swow\Thread(static function (Swow\Semaphore $semaphore): bool {
        return $semaphore->acquire(1000);
    }, $semaphore);
    $thread->start();
    $threads[] = $thread;
}

usleep(50000);
$semaphore->release(2);

foreach ($threads as $thread) {
    var_dump($thread->wait());
}
?>
--EXPECT--
bool(true)
bool(true)
