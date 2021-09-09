--TEST--
swow_coroutine: kill main by self
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

register_shutdown_function(function (){
    echo 'Done' . PHP_LF;
});

Swow\Coroutine::getCurrent()->kill();

echo 'Never here' . PHP_LF;

?>
--EXPECT--
Done
