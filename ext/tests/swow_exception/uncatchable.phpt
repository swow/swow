--TEST--
swow_exceptions: uncatchable
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

try {
    ((function () {
        try {
            (function () {
                try {
                    throw new class extends Exception implements Swow\Uncatchable
                    {

                    };
                    var_dump('never here');
                } catch (Throwable $e) {
                    var_dump($e);
                }
                var_dump('never here');
            })();
        } catch (Throwable $e) {
            var_dump($e);
        }
        var_dump('never here');
    })());
} catch (Throwable $e) {
    var_dump($e);
}
var_dump('never here');

?>
--EXPECTF--
Fatal error: Uncaught %s@anonymous in %s:%d
Stack trace:
#0 %s(%d): {closure}()
#1 %s(%d): {closure}()
#2 {main}
  thrown in %s on line %d
