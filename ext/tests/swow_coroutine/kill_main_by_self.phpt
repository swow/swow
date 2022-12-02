--TEST--
swow_coroutine: kill main by self
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

register_shutdown_function(static function (): void {
    echo "Done\n";
});

Swow\Coroutine::getCurrent()->kill();

echo "Never here\n";

?>
--EXPECT--
Done
