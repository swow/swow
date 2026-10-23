--TEST--
swow_mutex: lock, unlock and cross-thread handoff
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

$mutex = new Swow\Mutex();

var_dump($mutex->lock(0));
var_dump($mutex->lock(0));
$mutex->unlock();
var_dump($mutex->lock(0));
$mutex->unlock();

$mutex->lock();
$thread = new Swow\Thread(static function (Swow\Mutex $mutex): bool {
    return $mutex->lock(10);
}, $mutex);
$thread->start();
var_dump($thread->wait());
$mutex->unlock();

$mutex->lock();
$thread = new Swow\Thread(static function (Swow\Mutex $mutex): string {
    try {
        $mutex->unlock();
        return 'unlocked';
    } catch (Swow\SyncException $e) {
        return 'rejected';
    }
}, $mutex);
$thread->start();
var_dump($thread->wait());
$mutex->unlock();

$mutex->lock();
$thread = new Swow\Thread(static function (Swow\Mutex $mutex): bool {
    if (!$mutex->lock(1000)) {
        return false;
    }
    $mutex->unlock();

    return true;
}, $mutex);
$thread->start();
usleep(50000);
$mutex->unlock();
var_dump($thread->wait());

$mutex->lock();
var_dump($mutex->__debugInfo());
$mutex->unlock();

try {
    $mutex->unlock();
} catch (Swow\SyncException $e) {
    echo "not owner\n";
}
?>
--EXPECT--
bool(true)
bool(false)
bool(true)
bool(false)
string(8) "rejected"
bool(true)
array(1) {
  ["locked"]=>
  bool(true)
}
not owner
