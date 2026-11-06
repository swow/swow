--TEST--
swow_process: ChildProcess cwd and env
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
skip_if(PHP_OS_FAMILY === 'Windows', 'POSIX only');
/*
 * glibc reads its own environ when it runs the program, and a non-PIE executable
 * keeps that definition in its own image, so a child that is spawned with a
 * working directory does not receive the requested environment on the platforms
 * that use the fork path of libuv. /proc/self/exe is readable only on Linux,
 * which is the only platform where that path is taken.
 */
$header = @file_get_contents('/proc/self/exe', false, null, 0, 20);
if (is_string($header) && strlen($header) === 20 && substr($header, 0, 4) === "\x7fELF") {
    /* e_type sits at offset 16, in the byte order that EI_DATA declares */
    $type = unpack(ord($header[5]) === 2 ? 'n' : 'v', substr($header, 16, 2))[1];
    skip_if($type === 2 /* ET_EXEC */, 'the executable is not position independent');
}
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Process\ChildProcess;

$cwd = realpath(sys_get_temp_dir());
$process = ChildProcess::spawn([
    PHP_BINARY, '-n', '-r', 'fwrite(STDOUT, getcwd() . "|" . getenv("SWOW_CHILD_ENV"));',
], [
    'cwd' => $cwd,
    'env' => ['SWOW_CHILD_ENV' => 'yes'],
    'stdout' => 'pipe',
]);

$out = stream_get_contents($process->getStdout());
$process->wait();

Assert::same($out, $cwd . '|yes');
echo "Done\n";

?>
--EXPECT--
Done
