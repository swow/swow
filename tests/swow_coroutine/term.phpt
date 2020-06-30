--TEST--
swow_coroutine: term
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

$coroutine = new Swow\Coroutine(function () {
    try {
        Swow\Coroutine::yield();
        echo 'Never here';
    } catch (Swow\Coroutine\CrossException $error) {
        var_dump($error->getMessage());
        var_dump($error->getCode());
        if (
            $error->getFile() === __FILE__ &&
            $error->getLine() === __LINE__ - 7 &&
            $error->getPrevious()->getFile() === __FILE__ &&
            $error->getPrevious()->getLine() === __LINE__ + 7
        ) {
            echo 'Success' . PHP_LF;
        }
    }
});
$coroutine->resume();
$coroutine->term('Termination', 123);

?>
--EXPECT--
string(11) "Termination"
int(123)
Success
