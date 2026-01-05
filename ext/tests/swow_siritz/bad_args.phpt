--TEST--
swow_siritz: bad args
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';

skip_if_not_zts();
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Siritz;

echo "Create siritz normal\n";

// normal
new Siritz(function () {
    msleep(50);
    echo "I'm in siritz1!\n";
});

echo "Create siritz arg unserializable\n";

class Unserializable
{
    public function __serialize(): array
    {
        throw new Exception("Unserializable");
    }
}

Assert::throws(function () {
    new Siritz(function ($i) {
        echo "I'm in siritz2!\n";
    }, new Unserializable());
});

echo "Create siritz callable unserializable\n";

Assert::throws(function () {
    new Siritz(new Unserializable());
}, TypeError::class);

echo "Create siritz callable with bad arg\n";

$siritz = new Siritz(function ($i) {
    echo "I'm in siritz3!\n";
});

$siritz->run();
$siritz->wait();

echo "Done\n";
?>
--EXPECTF--
Create siritz normal
Create siritz arg unserializable
Create siritz callable unserializable
Create siritz callable with bad arg
%AFatal error: Uncaught ArgumentCountError: Too few arguments to function Closure::{closur%s}(), 0 passed and exactly 1 expected in %s
Stack trace:
#0 [internal function]: Closure->{closur%s}()
#1 {main}
  thrown in %s
Done
