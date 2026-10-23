--TEST--
swow_process: ChildProcess reports exit without wait
--SKIPIF--
<?php
if (getenv('SWOW_TSAN') !== false) {
    die('skip TSan cannot handle PHP bailout');
}
require __DIR__ . '/../include/skipif.php';
skip_if(PHP_OS_FAMILY === 'Windows', 'POSIX implementation only');
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Process\ChildProcess;

$process = ChildProcess::spawn([PHP_BINARY, '-n', '-r', 'exit(0);']);

$channel = new Swow\Channel(1);
$exited = false;
for ($i = 0; $i < 200; $i++) {
    if ($process->hasExited()) {
        $exited = true;
        break;
    }
    try {
        $channel->pop(10);
    } catch (Swow\ChannelException) {
    }
}

Assert::true($exited);
Assert::same($process->wait()->getExitCode(), 0);
echo "Done\n";

?>
--EXPECT--
Done
