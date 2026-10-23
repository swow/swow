--TEST--
swow_thread: concurrent start and wait stress
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

$threads = [];
for ($i = 0; $i < 16; $i++) {
    $thread = new Swow\Thread('array_sum', [1, 2]);
    $thread->start();
    $threads[] = $thread;
}

$failures = 0;
foreach ($threads as $thread) {
    if ($thread->wait() !== 3 || $thread->wait() !== 3 || $thread->getExitStatus() !== 0) {
        $failures++;
    }
}

var_dump($failures);
?>
--EXPECT--
int(0)
