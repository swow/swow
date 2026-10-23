--TEST--
swow_thread: repeated internal callable start and wait
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

$failures = 0;
for ($i = 0; $i < 100; $i++) {
    $thread = new Swow\Thread('array_sum', [1, 2]);
    $thread->start();
    if ($thread->wait() !== 3) {
        $failures++;
    }
    if ($thread->wait() !== 3) {
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
