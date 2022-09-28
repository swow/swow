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
    echo "Kill main\n";
    $main->kill();
    echo "Never here\n";
});

usleep(1000 + 1);
echo "Never here\n";

?>
--EXPECT--
Kill main
