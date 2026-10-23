--TEST--
swow_thread: wait returns the result
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

$thread = new Swow\Thread('array_sum', [1, 2]);
$thread->start();
var_dump($thread->wait());
var_dump($thread->wait());
var_dump($thread->getExitStatus());
?>
--EXPECT--
int(3)
int(3)
int(0)
