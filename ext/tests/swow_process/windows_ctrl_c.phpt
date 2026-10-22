--TEST--
swow_process: Windows SIGINT maps to CTRL_C_EVENT
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
skip_if(PHP_OS_FAMILY !== 'Windows', 'Windows only');
skip_if(!function_exists('sapi_windows_set_ctrl_handler'), 'needs sapi_windows_set_ctrl_handler');
require_once __DIR__ . '/../include/bootstrap.php';
try {
    php_options_with_swow();
} catch (Throwable) {
    skip('cannot run with swow');
}
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Process\ChildProcess;

$args = array_merge([test_php_path()], php_options_with_swow(), [
    '-r',
    'if (!sapi_windows_set_ctrl_handler(static function (): void { exit(0); })) {'
    . ' fwrite(STDOUT, "handler-failed\n"); exit(1); }'
    . ' fwrite(STDOUT, "ready\n"); fflush(STDOUT); while (true) { usleep(10000); }',
]);
$process = ChildProcess::spawn($args, [
    'stdout' => 'pipe',
]);

$stdout = $process->getStdout();
Assert::same(trim(fgets($stdout)), 'ready');
$process->kill(\Swow\Process\SIGINT);
$status = $process->wait(2000);

Assert::same($status->getExitCode(), 0);
echo "Done\n";

?>
--EXPECT--
Done
