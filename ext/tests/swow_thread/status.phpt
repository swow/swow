--TEST--
swow_thread: status
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

$thread = new Swow\Thread(static function (): void {
    usleep(100000);
});

var_dump($thread->isStarted());
var_dump($thread->isRunning());
var_dump($thread->isDetached());
var_dump($thread->getId());

$thread->start();

var_dump($thread->isStarted());
var_dump($thread->isRunning());
var_dump($thread->isDetached());
var_dump($thread->getId() > 0);

$thread->wait();

var_dump($thread->isRunning());
?>
--EXPECT--
bool(false)
bool(false)
bool(false)
int(0)
bool(true)
bool(true)
bool(false)
bool(true)
bool(false)
