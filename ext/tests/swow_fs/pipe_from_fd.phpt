--TEST--
swow_fs: pipe_from_fd
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Coroutine;
use Swow\Sync\WaitReference;

use function Swow\fileno;
use function Swow\pipe;
use function Swow\pipe_from_fd;

[$r, $w] = pipe();

$r2 = pipe_from_fd(fileno($r), 'rb');
$w2 = pipe_from_fd(fileno($w), 'wb');

$wr = new WaitReference();
Coroutine::run(static function () use ($w2, $wr): void {
    fwrite($w2, 'Hello, world!');
});

$data = fread($r, 1024);
var_dump($data);

WaitReference::wait($wr);

$wr = new WaitReference();
Coroutine::run(static function () use ($r2, $wr): void {
    $data = fread($r2, 1024);
    var_dump($data);
});

fwrite($w2, 'World, hello!');

WaitReference::wait($wr);

// do not do this
fclose($r2);
class TestError extends Error
{
}
$false = @fread($r, 1024);
Assert::eq($false, false);
set_error_handler(static function ($errno, $errstr, $errfile, $errline): void {
    throw new TestError($errstr, $errno);
}, E_WARNING | E_NOTICE);
Assert::throws(static function () use ($r): void {
    fread($r, 1024);
}, TestError::class);
restore_error_handler();
fclose($r);
fclose($w);
fclose($w2);

$file = fopen(__DIR__ . DIRECTORY_SEPARATOR . 'pipe_from_fd_test.txt', 'w+');
Assert::throws(static function () use ($file): void {
    pipe_from_fd(fileno($file), 'rb');
}, Swow\Exception::class, expectMessage: 'File descriptor is not a pipe');
fclose($file);

echo 'Done' . PHP_EOL;
?>
--CLEAN--
<?php
@unlink(__DIR__ . DIRECTORY_SEPARATOR . 'pipe_from_fd_test.txt');
?>
--EXPECT--
string(13) "Hello, world!"
string(13) "World, hello!"
Done
