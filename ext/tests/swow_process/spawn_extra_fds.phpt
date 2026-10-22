--TEST--
swow_process: ChildProcess multiple extra fds
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Process\ChildProcess;

use function Swow\pipe;

$pipe3 = pipe();
$pipe5 = pipe();
Assert::true(is_array($pipe3));
Assert::true(is_array($pipe5));
[$read3, $write3] = $pipe3;
[$read5, $write5] = $pipe5;

$process = ChildProcess::spawn([
    PHP_BINARY, '-n', '-r',
    '$a = stream_get_contents(fopen("php://fd/3", "rb"));'
    . ' $b = stream_get_contents(fopen("php://fd/5", "rb"));'
    . ' fwrite(STDOUT, $a . $b);',
], [
    'stdout' => 'pipe',
    'fds' => [3 => $read3, 5 => $read5],
]);

fclose($read3);
fclose($read5);
fwrite($write3, 'three');
fclose($write3);
fwrite($write5, 'five');
fclose($write5);

$stdout = stream_get_contents($process->getStdout());
$process->wait();

Assert::same($stdout, 'threefive');
echo "Done\n";

?>
--EXPECT--
Done
