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
    'printf("%d" . PHP_EOL, getmypid());' .
        'Swow\Coroutine::yield();' .
        'fwrite(STDOUT, "Never here" . PHP_EOL);',
], static function ($proc, array $pipes): void {
    $pid = (int) trim(fgets($pipes[1], 4096));
    Assert::notSame($pid, 0);
    echo trim(fgets($pipes[1], 4096)) . "\n";
    Signal::kill($pid, Signal::TERM);
    while (!feof($pipes[1])) {
        echo trim(fgets($pipes[1], 4096)) . "\n";
    }
});

echo "Done\n";

?>
--EXPECT--
Warning: <COROUTINE> Deadlock: all coroutines are asleep in scheduler

Done
