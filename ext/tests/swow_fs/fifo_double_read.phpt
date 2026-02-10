--TEST--
swow_fs: FIFO double read
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
require __DIR__ . '/../include/bootstrap.php';

// now, cat_fs_open() cannot open fifo on windows as a plain file
skip_if_win();
skip_if_cannot_create_fifo();
?>
--INI--
ffi.enable=1
--XFAIL--
Need to fix
--FILE--
<?php

require __DIR__ . '/../include/bootstrap.php';

use Swow\Coroutine;
use Swow\Sync\WaitReference;

$pipePath = make_fifo();

$wr = new WaitReference();

Coroutine::run(static function () use ($pipePath, &$w, $wr): void {
    $w = fopen($pipePath, 'w');
});
$r = fopen($pipePath, 'r');
WaitReference::wait($wr);

$wr = new WaitReference();

Coroutine::run(static function () use ($w, $r, $wr): void {
    fwrite($w, 'Hello, world!'); // 1. on half of fwrite calling, coroutine yield
    $read = fread($r, 13); // 3. duplicate read on $r, things got corrupted
    var_dump($read);
});

$read = fread($r, 13); // 2. on half of fread calling, coroutine yield
var_dump($read);
fwrite($w, 'World, hello!');

WaitReference::wait($wr);

fclose($r);
fclose($w);
if (PHP_OS_FAMILY !== 'Windows') {
    // on windows, closed fifo will be deleted automatically
    @unlink($pipePath);
}

echo 'Done' . PHP_EOL;
?>
--EXPECT--
string(13) "Hello, world!"
string(13) "World, hello!"
Done
