--TEST--
swow_process: closing a running child leaves no zombie behind
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
if (!is_dir('/proc')) {
    skip('This test needs procfs');
}
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Process\ChildProcess;

$pids = [];
for ($i = 0; $i < 5; $i++) {
    $process = ChildProcess::spawn([PHP_BINARY, '-n', '-r', 'usleep(300000);']);
    $pids[] = $process->getPid();
    // closing or abandoning a running child must not leave the child unreaped
    unset($process);
}

sleep(1);

$zombies = 0;
foreach ($pids as $pid) {
    $stat = @file_get_contents("/proc/{$pid}/stat");
    if (is_string($stat) && preg_match('/^\d+ \(.*\) Z /', $stat)) {
        $zombies++;
    }
}
Assert::same($zombies, 0);

echo "Done\n";

?>
--EXPECT--
Done
