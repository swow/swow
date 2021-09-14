--TEST--
swow_signal: Swow\Signal::kill
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Signal;

$proc = proc_open(
    PHP_BINARY . ' -r "echo \'start\'.PHP_EOL;sleep(10);echo \'Never here\'.PHP_EOL;"',
    [
        0 => STDIN,
        1 => ['pipe', 'w'],
        2 => STDERR,
    ],
    $pipes
);

Assert::same(fread($pipes[1], 4096), 'start' . PHP_EOL);

$status = proc_get_status($proc);

Signal::kill($status['pid'], Signal::TERM);

while (($status = proc_get_status($proc)) && $status['running']) {
    msleep(20);
}

echo 'Done' . PHP_LF;
?>
--EXPECT--
Done

