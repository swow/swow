--TEST--
swow_siritz: wait
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

echo "Create siritz1\n";

$siritz = new Siritz(function () {
    msleep(50);
    echo "I'm in siritz1!\n";
});

echo "Start siritz1\n";

$siritz->run();

$siritz->wait();

echo "Done1\n";

echo "Create siritz2\n";

$siritz = new Siritz(function () {
    msleep(100);
});

echo "Start siritz2\n";

$siritz->run();

Assert::throws(function () use ($siritz) {
    $siritz->wait(10);
}, Swow\SiritzException::class, 0, 'Wait for thread timed out');

echo "Done2\n";

echo "Create siritz3\n";

$siritz = new Siritz(function () {
    // do nothing
});

echo "Start siritz3\n";

$siritz->run();

msleep(50);

$siritz->wait();

echo "Done3\n";

echo "Create siritz4\n";

$siritz = new Siritz(function () {
    msleep(100);
});

echo "Start siritz4\n";

$siritz->run();

$siritz->wait(10, true);

echo "Done4\n";

echo "Create siritz5\n";

$siritz = new Siritz(function () {
    msleep(100);
});

Assert::throws(function () use ($siritz) {
    $siritz->wait(10, true);
}, Swow\SiritzException::class, 0, 'Thread not started');

echo "Done5\n";
?>
--EXPECT--
Create siritz1
Start siritz1
I'm in siritz1!
Done1
Create siritz2
Start siritz2
Done2
Create siritz3
Start siritz3
Done3
Create siritz4
Start siritz4
Done4
Create siritz5
Done5
