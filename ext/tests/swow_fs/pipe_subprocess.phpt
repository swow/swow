--TEST--
swow_fs: pipe subprocess
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';

skip_if_extension_not_exist('pcntl');
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use function Swow\pipe;

[$r, $w] = pipe();

// fstat
$stat = fstat($r);
Assert::eq($stat['mode'] & 0x1000, 0x1000, 'r is a pipe');

$stat = fstat($w);
Assert::eq($stat['mode'] & 0x1000, 0x1000, 'w is a pipe');

// common use
$pid = pcntl_fork();
if ($pid === 0) {
    // child
    fwrite($w, 'Hello from child');
    fclose($w);
    $data = fread($r, 1024);
    var_dump($data);
    fclose($r);
} else {
    // parent
    $data = fread($r, 1024);
    var_dump($data);
    fclose($r);
    fwrite($w, 'Hello from parent');
    fclose($w);
}

if ($pid !== 0) {
    pcntl_waitpid($pid, $status);
}

echo 'Done' . PHP_EOL;
?>
--EXPECT--
string(16) "Hello from child"
string(17) "Hello from parent"
Done
Done
