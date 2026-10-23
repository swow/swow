--TEST--
swow_thread: repeated start/wait stress
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

$failures = 0;
for ($i = 0; $i < 50; $i++) {
    $thread = new Swow\Thread(static function () use ($i): int {
        return $i;
    });
    $thread->start();
    if ($thread->wait() !== $i) {
        $failures++;
    }
    if ($thread->wait() !== $i) {
        $failures++;
    }
    if ($thread->getExitStatus() !== 0) {
        $failures++;
    }
}

var_dump($failures);
?>
--EXPECT--
int(0)
