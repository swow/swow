--TEST--
swow_process: ChildProcess stdout resource redirection
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
$handle = fopen($file, 'w');
Assert::true(is_resource($handle));

$process = ChildProcess::spawn([
    PHP_BINARY, '-n', '-r', 'fwrite(STDOUT, "resource");',
], [
    'stdout' => $handle,
]);

$process->wait();
fclose($handle);

Assert::same(file_get_contents($file), 'resource');
Assert::null($process->getStdout());
unlink($file);
echo "Done\n";

?>
--EXPECT--
Done
