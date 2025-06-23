--TEST--
swow_siritz: create
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Siritz;

echo "Create siritz\n";

$siritz = new Siritz(function () {
    msleep(50);
    echo "I'm in siritz!\n";
});

echo "Start siritz\n";

$siritz->run();

?>
--EXPECT--
Create siritz
Start siritz
I'm in siritz!
