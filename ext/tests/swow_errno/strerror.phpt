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

Assert::same(Errno::getDescriptionOf(0), 'Unknown error');
Assert::same(Errno::getDescriptionOf(Errno::EAGAIN), 'Resource temporarily unavailable');
Assert::same(Errno::getDescriptionOf(2147483647), 'Unknown error');
if (PHP_INT_MAX > 2147483647) {
    try {
        $ret = Errno::getDescriptionOf(PHP_INT_MAX);
        var_dump('bad return', $ret);
    } catch (ValueError $e) {
        echo $e->getMessage(), "\n";
    }
}

echo "Done\n";
?>
--EXPECT--
Swow\Errno::getDescriptionOf(): Argument #1 ($error) Errno passed in is not in errno_t range
Done
