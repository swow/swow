--TEST--
swow_semaphore: acquire, release and cross-thread handoff
--SKIPIF--
<?php
if (!PHP_ZTS) {
    die('skip ZTS only');
}
require __DIR__ . '/../include/skipif.php';
?>
--INI--
swow.closure_serializer=1
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

$semaphore = new Swow\Semaphore(1);

var_dump($semaphore->acquire(0));
var_dump($semaphore->acquire(0));
$semaphore->release();
var_dump($semaphore->acquire(0));
$semaphore->release();

$semaphore->release(2);
var_dump($semaphore->acquire(0));
var_dump($semaphore->acquire(0));
var_dump($semaphore->acquire(0));
var_dump($semaphore->acquire(0));
$semaphore->release(2);

$copy = unserialize(serialize($semaphore));
$copy->release();
var_dump($semaphore->acquire(0));

$semaphore = new Swow\Semaphore(0);
$thread = new Swow\Thread(static function (Swow\Semaphore $semaphore): bool {
    return $semaphore->acquire(10);
}, $semaphore);
$thread->start();
var_dump($thread->wait());

$semaphore = new Swow\Semaphore(0);
$thread = new Swow\Thread(static function (Swow\Semaphore $semaphore): bool {
    if (!$semaphore->acquire(1000)) {
        return false;
    }
    $semaphore->release();

    return true;
}, $semaphore);
$thread->start();
usleep(50000);
$semaphore->release();
var_dump($thread->wait());

try {
    new Swow\Semaphore(-1);
} catch (ValueError $e) {
    echo "value error\n";
}

try {
    $semaphore->release(-1);
} catch (ValueError $e) {
    echo "release error\n";
}
?>
--EXPECT--
bool(true)
bool(false)
bool(true)
bool(true)
bool(true)
bool(true)
bool(false)
bool(true)
bool(false)
bool(true)
value error
release error
