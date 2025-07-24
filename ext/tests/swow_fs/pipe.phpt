--TEST--
swow_fs: pipe
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Coroutine;
use Swow\Sync\WaitReference;

use function Swow\pipe;

[$r, $w] = pipe();

$wr = new WaitReference();

// common use
Coroutine::run(static function () use ($w, $wr): void {
    fwrite($w, 'Hello, world!');
});

$data = fread($r, 1024);
var_dump($data);

// wait for coroutine to finish, avoid double read
WaitReference::wait($wr);

// fstat
$stat = fstat($r);
Assert::eq($stat['mode'] & 0x1000, /* S_IFIFO */ 0x1000, 'r is a pipe');

$stat = fstat($w);
Assert::eq($stat['mode'] & 0x1000, /* S_IFIFO */ 0x1000, 'w is a pipe');

// fclose
fclose($r);
fclose($w);

echo 'Done' . PHP_EOL;
?>
--EXPECT--
string(13) "Hello, world!"
Done
