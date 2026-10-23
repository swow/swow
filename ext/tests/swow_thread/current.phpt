--TEST--
swow_thread: getCurrent in the main thread
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

$current = Swow\Thread::getCurrent();

var_dump($current instanceof Swow\Thread);
var_dump($current === Swow\Thread::getCurrent());
var_dump($current->isStarted());
var_dump($current->isRunning());
var_dump($current->isDetached());
var_dump($current->getId() > 0);
var_dump($current->getId() === Swow\Thread::getCurrentId());
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
bool(true)
bool(false)
bool(true)
bool(true)
