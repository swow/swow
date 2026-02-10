--TEST--
swow_misc: nproc
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

var_dump(Swow\NPROC_AVAILABLE);
var_dump(Swow\nproc());
var_dump(Swow\nproc(Swow\NPROC_AVAILABLE));

Assert::throws(static function (): void {
    var_dump(Swow\nproc(-1));
}, Swow\Exception::class);

echo "Done\n";
?>
--EXPECTF--
int(0)
int(%d)
int(%d)
Done
