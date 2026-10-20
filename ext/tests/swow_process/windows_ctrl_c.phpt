--TEST--
swow_process: Windows SIGINT maps to CTRL_C_EVENT
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
skip_if(!class_exists(\Swow\Process\ChildProcess::class), 'Windows ChildProcess is not implemented yet');
skip_if(PHP_OS_FAMILY !== 'Windows', 'Windows only');
skip_if(!function_exists('sapi_windows_set_ctrl_handler'), 'needs sapi_windows_set_ctrl_handler');
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Process\ChildProcess;

$process = ChildProcess::spawn([
    PHP_BINARY, '-n', '-r',
    'sapi_windows_set_ctrl_handler(static function (): void { exit(0); }); sleep(5);',
]);
$process->kill(\Swow\Process\SIGINT);
$status = $process->wait(2000);

Assert::same($status->getExitCode(), 0);
echo "Done\n";

?>
--EXPECT--
Done
