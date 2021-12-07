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

$other_options = PHP_OS_FAMILY === 'Windows' ? [ 'bypass_shell' => true ] : null;

$proc = proc_open(
    PHP_BINARY . ' -r "echo \'start\'.PHP_EOL;sleep(10);fwrite(STDERR, \'Never here\'.PHP_EOL);"',
    [
        0 => STDIN,
        1 => ['pipe', 'w'],
        2 => STDERR,
    ],
    $pipes,
    null,
    null,
    $other_options
);

Assert::same(fread($pipes[1], 4096), 'start' . PHP_EOL);

$status = proc_get_status($proc);

Signal::kill($status['pid'], Signal::TERM);

$exitStatus = proc_close($proc);

if ($exitStatus !== null) {
    // "If PHP has been compiled with --enable-sigchild, the return value of this function is undefined."
    Assert::same($exitStatus, PHP_OS_FAMILY === 'Windows' ? 1 : Signal::TERM);
}

echo 'Done' . PHP_LF;
?>
--EXPECT--
Done
