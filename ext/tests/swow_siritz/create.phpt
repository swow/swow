--TEST--
swow_siritz: create
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';

skip_if_not_zts();
?>
--INI--
swow.closure_serializer=1
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Siritz;

echo "Create siritz\n";

$siritz = new Siritz(function () {
    for ($i = 0; $i < 100; $i++) {
        // main thread will interrupt this thread using unwind exit
        // so we need to split sleep into small chunks
        msleep(50);
    }
    echo "I'm in siritz!\n";
});

echo "Start siritz\n";

$siritz->run();

?>
--EXPECT--
Create siritz
Start siritz
