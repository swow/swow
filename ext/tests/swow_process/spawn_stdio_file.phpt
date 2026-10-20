--TEST--
swow_process: ChildProcess stdout file redirection
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
skip_if(PHP_OS_FAMILY === 'Windows', 'POSIX implementation only');
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Process\ChildProcess;

$file = tempnam(sys_get_temp_dir(), 'swow-child-');
Assert::true(is_string($file));

$process = ChildProcess::spawn([
    PHP_BINARY, '-n', '-r', 'fwrite(STDOUT, "file");',
], [
    'stdout' => ['file', $file, 'w'],
]);

$process->wait();

Assert::same(file_get_contents($file), 'file');
Assert::null($process->getStdout());
unlink($file);
echo "Done\n";

?>
--EXPECT--
Done
