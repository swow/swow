--TEST--
swow_process: waiting for one child does not take the exit status of other children
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
skip_if_win();
skip_if_function_not_exist('proc_open');
skip_if_function_not_exist('popen');
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Coroutine;

/*
 * A supervisor coroutine waits in proc_close() for a long running worker, as a
 * process manager does. Meanwhile request handlers run short commands in the
 * ways PHP code usually does. Each of them must get the exit code of its own
 * child, even though their SIGCHLD arrives while the supervisor is waiting.
 */
$php = escapeshellarg(PHP_BINARY) . ' -n -r ';
$settle = static fn() => usleep(100 * 1000); // let SIGCHLD reach the event loop first

$worker = proc_open([PHP_BINARY, '-n', '-r', 'usleep(1500 * 1000); exit(9);'], [], $pipes);
$workerExitCode = null;
$supervisor = Coroutine::run(static function () use ($worker, &$workerExitCode): void {
    $workerExitCode = proc_close($worker);
});
$settle();
Assert::true($supervisor->isAlive(), 'the supervisor must still be waiting');

$pipe = popen($php . escapeshellarg('exit(5);'), 'r');
$settle();
echo 'pclose: ', pclose($pipe), "\n";

exec($php . escapeshellarg('echo "out"; exit(3);'), $output, $resultCode);
echo 'exec: ', json_encode($output), ' ', $resultCode, "\n";

echo 'shell_exec: ', trim((string) shell_exec($php . escapeshellarg('echo "shell";'))), "\n";

$process = proc_open([PHP_BINARY, '-n', '-r', 'exit(6);'], [], $pipes);
do {
    $status = proc_get_status($process);
    if ($status['running']) {
        usleep(10 * 1000);
    }
} while ($status['running']);
echo 'proc_get_status: ', $status['exitcode'], "\n";
proc_close($process);

if (function_exists('pcntl_waitpid')) {
    $process = proc_open([PHP_BINARY, '-n', '-r', 'exit(7);'], [], $pipes);
    $pid = proc_get_status($process)['pid'];
    $settle();
    $waited = pcntl_waitpid($pid, $waitStatus);
    echo 'pcntl_waitpid: ', $waited === $pid ? 'pid' : var_export($waited, true), ' ', pcntl_wexitstatus($waitStatus), "\n";
    unset($process);
} else {
    echo "pcntl_waitpid: pid 7\n";
}

while ($supervisor->isAlive()) {
    usleep(50 * 1000);
}
echo 'supervisor: ', $workerExitCode, "\n";
?>
--EXPECT--
pclose: 5
exec: ["out"] 3
shell_exec: shell
proc_get_status: 6
pcntl_waitpid: pid 7
supervisor: 9
