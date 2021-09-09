--TEST--
swow_errno: strerror
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use function Swow\Errno\strerror;

Assert::same(strerror(0), 'Unknown error');
Assert::same(strerror(Swow\Errno\EAGAIN), 'Resource temporarily unavailable');
Assert::same(strerror(2147483647), 'Unknown error');
if (PHP_INT_MAX > 2147483647) {
    try {
        $ret = strerror(PHP_INT_MAX);
        var_dump('bad return', $ret);
    } catch (ValueError $e) {
        Assert::same($e->getMessage(), 'Errno passed in is not in errno_t range');
    }
}

echo "Done\n";
?>
--EXPECT--
Done
