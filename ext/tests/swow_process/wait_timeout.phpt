--TEST--
swow_process: wait() times out for a running child
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Process\ForkProcess;
use Swow\Process\ProcessException;

$process = ForkProcess::fork(static function (): int {
    usleep(500000);
    return 42;
});

try {
    $process->wait(10);
    echo "No timeout\n";
} catch (ProcessException $exception) {
    echo "Timeout\n";
}

$process->kill();
$process->wait(-1);
echo "Done\n";

?>
--EXPECT--
Timeout
Done
