--TEST--
swow_thread: start fails when the callable cannot be serialized
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

$callable = Closure::bind(function (): void {
}, new stdClass(), null);

$thread = new Swow\Thread($callable);

try {
    $thread->start();
} catch (Throwable $e) {
    echo get_class($e), "\n";
}

var_dump($thread->isStarted());
?>
--EXPECT--
Error
bool(false)
