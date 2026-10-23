--TEST--
swow_thread: thread shutdown timeout ini
--SKIPIF--
<?php
if (!PHP_ZTS) {
    exit('skip ZTS only');
}
require __DIR__ . '/../include/skipif.php';
?>
--INI--
swow.thread_shutdown_timeout=-1
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

var_dump(ini_get('swow.thread_shutdown_timeout'));

$old = ini_set('swow.thread_shutdown_timeout', '50');
var_dump($old);
var_dump(ini_get('swow.thread_shutdown_timeout'));

$thread = new Swow\Thread('ini_get', 'swow.thread_shutdown_timeout');
$thread->start();
var_dump($thread->wait());

var_dump(ini_set('swow.thread_shutdown_timeout', '-1'));
var_dump(ini_get('swow.thread_shutdown_timeout'));
?>
--EXPECT--
string(2) "-1"
string(2) "-1"
string(2) "50"
string(2) "-1"
string(2) "50"
string(2) "-1"
