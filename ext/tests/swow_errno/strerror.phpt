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

Assert::same(strerror(0), "Unknown error");
Assert::same(strerror(Swow\Errno\EAGAIN), "Resource temporarily unavailable");
Assert::same(strerror(PHP_INT_MAX), "Unknown error");

echo "done\n";
?>
--EXPECT--
done
