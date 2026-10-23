--TEST--
swow: serialized shared objects die with their last owner
--SKIPIF--
<?php
if (!PHP_ZTS) {
    die('skip ZTS only');
}
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

$mutex = new Swow\Mutex();
$data = serialize($mutex);
unset($mutex);

try {
    unserialize($data);
} catch (Swow\SyncException $e) {
    echo "mutex: ", $e->getMessage(), "\n";
}

$semaphore = new Swow\Semaphore(1);
$data = serialize($semaphore);
unset($semaphore);

try {
    unserialize($data);
} catch (Swow\SyncException $e) {
    echo "semaphore: ", $e->getMessage(), "\n";
}
?>
--EXPECT--
mutex: Serialized shared object is no longer alive
semaphore: Serialized shared object is no longer alive
