--TEST--
swow_coroutine: getCurrent
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

$coroutine = new Swow\Coroutine(static function () use (&$coroutine) {
    $coroutine_x = new Swow\Coroutine(static function (): void { });
    $self = Swow\Coroutine::getCurrent();
    if ($self === $coroutine && $self !== $coroutine_x) {
        return 'Success';
    }
    return 'Failed';
});
echo "Resume\n";
echo $coroutine->resume() . "\n";
echo "Done\n";

?>
--EXPECT--
Resume
Success
Done
