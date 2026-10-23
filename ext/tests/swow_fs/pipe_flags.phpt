--TEST--
swow_fs: pipe flags
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use function Swow\pipe;

[$r, $w] = pipe(0, 0);
var_dump(is_resource($r), is_resource($w));
fclose($r);
fclose($w);

[$r, $w] = pipe(0);
var_dump(is_resource($r), is_resource($w));
fclose($r);
fclose($w);

echo "Done\n";

?>
--EXPECT--
bool(true)
bool(true)
bool(true)
bool(true)
Done
