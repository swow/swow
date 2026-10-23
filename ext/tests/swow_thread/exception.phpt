--TEST--
swow_thread: exception from the thread
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
    throw new RuntimeException('boom', 42);
});
$thread->start();
try {
    $thread->wait();
} catch (Throwable $e) {
    echo get_class($e), ': ', $e->getMessage(), ' ', $e->getCode(), "\n";
}

try {
    $thread->wait();
} catch (Throwable $e) {
    echo get_class($e), ': ', $e->getMessage(), ' ', $e->getCode(), "\n";
}

var_dump($thread->getExitStatus());
?>
--EXPECT--
RuntimeException: boom 42
RuntimeException: boom 42
int(255)
