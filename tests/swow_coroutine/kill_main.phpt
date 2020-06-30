--TEST--
swow_coroutine: kill main
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

Swow\Coroutine::run(function () {
    $main = Swow\Coroutine::getCurrent()->getPrevious();
    usleep(1000);
    $main->kill();
    echo 'Done' . PHP_LF;
});

usleep(1000 + 1);
echo 'Never here' . PHP_LF;

?>
--EXPECT--
Done
