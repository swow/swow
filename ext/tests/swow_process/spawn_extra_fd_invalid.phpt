--TEST--
swow_process: ChildProcess extra fd invalid options
--SKIPIF--
<?php
if (getenv('SWOW_TSAN') !== false) {
    exit('skip TSan cannot handle PHP bailout');
}
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Process\ChildProcess;
use Swow\Process\ProcessException;

use function Swow\pipe;

$pipe = pipe();
Assert::true(is_array($pipe));
[$read, $write] = $pipe;

try {
    ChildProcess::spawn([PHP_BINARY, '-n', '-r', 'exit(0);'], ['fds' => 'bad']);
    echo "type\n";
} catch (ProcessException) {
    echo "type-ex\n";
}

try {
    ChildProcess::spawn([PHP_BINARY, '-n', '-r', 'exit(0);'], ['fds' => [0 => $read]]);
    echo "low\n";
} catch (ProcessException) {
    echo "low-ex\n";
}

try {
    ChildProcess::spawn([PHP_BINARY, '-n', '-r', 'exit(0);'], ['fds' => [300 => $read]]);
    echo "high\n";
} catch (ProcessException) {
    echo "high-ex\n";
}

try {
    ChildProcess::spawn([PHP_BINARY, '-n', '-r', 'exit(0);'], ['fds' => [3 => 'bad']]);
    echo "value\n";
} catch (ProcessException) {
    echo "value-ex\n";
}

fclose($read);
fclose($write);
echo "Done\n";

?>
--EXPECT--
type-ex
low-ex
high-ex
value-ex
Done
