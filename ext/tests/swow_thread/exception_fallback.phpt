--TEST--
swow_thread: exception class unknown to the parent falls back to ThreadException
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
    eval('class SwowThreadChildOnlyException extends \Exception {}');
    throw new SwowThreadChildOnlyException('child-only');
});
$thread->start();

try {
    $thread->wait();
} catch (Throwable $e) {
    echo get_class($e), ': ', $e->getMessage(), "\n";
}
?>
--EXPECT--
Swow\ThreadException: Thread returned a non-throwable exception
