--TEST--
swow_coroutine: deadlock
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Signal;

php_proc_with_swow([
    '-r',
    'echo getmypid() . PHP_EOL;' .
        'Swow\Coroutine::yield();' .
        'fwrite(STDERR, "Never here" . PHP_EOL);'
], function ($proc, array $pipes) {
    $pid = (int) trim(fgets($pipes[1], 4096));
    echo trim(fgets($pipes[2], 4096)) . PHP_LF;
    Signal::kill($pid, Signal::TERM);
});

echo 'Done' . PHP_LF;

?>
--EXPECT--
Warning: <COROUTINE> Deadlock: all coroutines are asleep in scheduler
Done
