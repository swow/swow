--TEST--
swow_coroutine: in_shutdown base
--SKIPIF--
<?php
require __DIR__ . '/../../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../../include/bootstrap.php';

use Swow\Coroutine;

register_shutdown_function(static function (): void {
    Coroutine::run(static function (): void {
        var_dump(file_get_contents('http://www.baidu.com'));
    });
});

?>
--EXPECT--
