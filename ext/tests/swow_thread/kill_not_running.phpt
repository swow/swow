--TEST--
swow_thread: kill a thread which is not running
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

$thread = new Swow\Thread('var_dump', 'x');

try {
    $thread->kill();
} catch (Swow\ThreadException $e) {
    echo "not started\n";
}

$thread->start();
$thread->wait();

try {
    $thread->kill();
} catch (Swow\ThreadException $e) {
    echo "not running\n";
}
?>
--EXPECT--
not started
string(1) "x"
not running
