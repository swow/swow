--TEST--
swow_thread: debug info
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

$thread = new Swow\Thread(static function (): string {
    usleep(200000);
    return strrev('abc');
});

var_dump($thread->__debugInfo()['started']);

$thread->start();
$info = $thread->__debugInfo();
var_dump($info['started']);
var_dump($info['running']);
var_dump($info['detached']);
var_dump($info['id'] > 0);

$thread->wait();
var_dump($thread->__debugInfo()['running']);
?>
--EXPECT--
bool(false)
bool(true)
bool(true)
bool(false)
bool(true)
bool(false)
