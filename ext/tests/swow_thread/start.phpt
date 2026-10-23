--TEST--
swow_thread: start and wait
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

$thread = new Swow\Thread('var_dump', 'child');
$thread->start();
$thread->wait();
echo "parent\n";
?>
--EXPECT--
string(5) "child"
parent
