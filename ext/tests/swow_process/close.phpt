--TEST--
swow_process: ChildProcess close running process
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
skip_if(PHP_OS_FAMILY === 'Windows', 'POSIX implementation only');
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Process\ChildProcess;
use Swow\Process\ProcessException;

use function Swow\Process\kill;

$process = ChildProcess::spawn([PHP_BINARY, '-n', '-r', 'sleep(5);'], [
    'stdout' => 'pipe',
]);
$pid = $process->getPid();

$process->close();
$process->close();
Assert::null($process->getStdout());

try {
    $process->getPid();
    echo "pid\n";
} catch (ProcessException) {
    echo "pid-ex\n";
}

try {
    $process->hasExited();
    echo "exited\n";
} catch (ProcessException) {
    echo "exited-ex\n";
}

try {
    $process->wait();
    echo "wait\n";
} catch (ProcessException) {
    echo "wait-ex\n";
}

try {
    $process->kill(\Swow\Process\SIGTERM);
    echo "kill\n";
} catch (ProcessException) {
    echo "kill-ex\n";
}

/* close leaves the process running */
kill($pid, 0);
kill($pid, \Swow\Process\SIGKILL);

$channel = new Swow\Channel(1);
for ($i = 0; $i < 200; $i++) {
    try {
        kill($pid, 0);
    } catch (ProcessException) {
        break;
    }
    try {
        $channel->pop(10);
    } catch (Swow\ChannelException) {
    }
}

echo "Done\n";

?>
--EXPECT--
pid-ex
exited-ex
wait-ex
kill-ex
Done
