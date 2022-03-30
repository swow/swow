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
    [
        real_php_path(),
        '-r',
        'echo getmypid().PHP_EOL;' .
            'sleep(10);' .
            'fwrite(STDERR, "Never here".PHP_EOL);',
    ], [
        0 => STDIN,
        1 => ['pipe', 'w'],
        2 => STDERR,
    ],
    $pipes,
);

$pid = (int)trim(fgets($pipes[1]));

Signal::kill($pid, Signal::TERM);

$exitStatus = proc_close($proc);

// "If PHP has been compiled with --enable-sigchild, the return value of this function is undefined."
Assert::same($exitStatus, PHP_OS_FAMILY === 'Windows' ? 1 : Signal::TERM);

echo 'Done' . PHP_LF;
?>
--EXPECT--
Done
