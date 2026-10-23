--TEST--
swow_thread: id is fixed after start
--SKIPIF--
<?php
if (!PHP_ZTS) {
    exit('skip ZTS only');
}
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

$thread = new Swow\Thread('usleep', 100000);

var_dump($thread->getId());

$thread->start();
$id = $thread->getId();
var_dump($id > 0);
var_dump($thread->__debugInfo()['id'] === $id);

$thread->wait();
var_dump($thread->getId() === $id);

$detached = new Swow\Thread('usleep', 100000);
$detached->start();
$detached_id = $detached->getId();
$detached->detach();
var_dump($detached->getId() === $detached_id);
?>
--EXPECT--
int(0)
bool(true)
bool(true)
bool(true)
bool(true)
