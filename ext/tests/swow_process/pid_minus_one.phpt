--TEST--
swow_process: wait() and kill() with pid -1 follow POSIX
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Process\ForkProcess;

// A ForkProcess created without fork() has pid -1: POSIX waitpid(-1) waits
// for any child, kill(-1, ...) signals all processes
$any = new ForkProcess();
Assert::same($any->getPid(), -1);
$any->kill(0);

$process = ForkProcess::fork(static function (): int {
    return 42;
});

$status = $any->wait(1000);
Assert::same($status->getExitCode(), 42);
echo "Done\n";

?>
--EXPECT--
Done
