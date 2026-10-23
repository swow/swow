--TEST--
swow_thread: operations on the current thread
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

try {
    $current->start();
} catch (Swow\ThreadException $e) {
    echo "start: ", $e->getMessage(), "\n";
}

try {
    $current->wait();
} catch (Swow\ThreadException $e) {
    echo "wait: ", $e->getMessage(), "\n";
}

try {
    $current->detach();
} catch (Swow\ThreadException $e) {
    echo "detach: ", $e->getMessage(), "\n";
}

try {
    $current->kill();
} catch (Swow\ThreadException $e) {
    echo "kill: ", $e->getMessage(), "\n";
}
?>
--EXPECT--
start: Thread has already been started
wait: Cannot wait for the current thread
detach: Cannot detach the current thread
kill: Cannot kill the current thread
