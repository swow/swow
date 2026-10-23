--TEST--
swow_thread: run
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

$thread = Swow\Thread::run('strrev', 'abc');

var_dump($thread instanceof Swow\Thread);
var_dump($thread->isStarted());
var_dump($thread->wait());
?>
--EXPECT--
bool(true)
bool(true)
string(3) "cba"
