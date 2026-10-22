--TEST--
swow_process: ChildProcess extra fd
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Process\ChildProcess;

use function Swow\pipe;

$pipe = pipe();
Assert::true(is_array($pipe));
[$read, $write] = $pipe;

$process = ChildProcess::spawn([
    PHP_BINARY, '-n', '-r',
    '$f = fopen("php://fd/3", "rb"); fwrite(STDOUT, stream_get_contents($f));',
], [
    'stdout' => 'pipe',
    'fds' => [3 => $read],
]);

fclose($read);
fwrite($write, 'hello');
fclose($write);

$stdout = stream_get_contents($process->getStdout());
$process->wait();

Assert::same($stdout, 'hello');
echo "Done\n";

?>
--EXPECT--
Done
