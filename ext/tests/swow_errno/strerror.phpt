--TEST--
swow_errno: strerror
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Errno;

Assert::same(Errno::getDescriptionFor(0), 'Unknown error');
Assert::same(Errno::getDescriptionFor(Errno::EAGAIN), 'Resource temporarily unavailable');
Assert::same(Errno::getDescriptionFor(2147483647), 'Unknown error');
if (PHP_INT_MAX > 2147483647) {
    try {
        $ret = Errno::getDescriptionFor(PHP_INT_MAX);
        var_dump('bad return', $ret);
    } catch (ValueError $e) {
        Assert::same($e->getMessage(), 'Errno passed in is not in errno_t range');
    }
}

echo "Done\n";
?>
--EXPECT--
Done
