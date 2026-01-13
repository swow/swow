--TEST--
swow_siritz: tid
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

var_dump(getmytid());

$siritz = new Siritz(function () {
    var_dump(getmytid());
});

Assert::throws(function () use ($siritz) {
    $siritz->getTid();
}, Swow\SiritzException::class, 0, 'Thread not started');

$siritz->run();

var_dump($siritz->getTid());

$siritz->wait();

echo "Done\n";
?>
--EXPECTF--
int(%d)
int(%d)
int(%d)
Done
