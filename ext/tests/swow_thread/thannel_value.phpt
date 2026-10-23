--TEST--
swow_thannel: values are copies and both directions work
--SKIPIF--
<?php
if (!PHP_ZTS) {
    die('skip ZTS only');
}
require __DIR__ . '/../include/skipif.php';
?>
--INI--
swow.closure_serializer=1
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

class SwowThannelBadValue
{
    public function __serialize(): array
    {
        throw new Exception('bad value');
    }
}

$object = new stdClass();
$object->value = 1;
$thannel = new Swow\Thannel(1);
$thannel->push($object);
$object->value = 2;
$copy = $thannel->pop(0);
var_dump($copy->value);
var_dump($copy !== $object);

$thannel = new Swow\Thannel(1);
$thannel->push('a');
try {
    $thannel->push('b', 10);
} catch (Swow\ThannelException $e) {
    echo "full timeout\n";
}

$thannel = new Swow\Thannel(0);
$thread = new Swow\Thread(static function (Swow\Thannel $thannel): string {
    return $thannel->pop(1000);
}, $thannel);
$thread->start();
usleep(50000);
$thannel->push('world');
var_dump($thread->wait());

$thannel = new Swow\Thannel(1);
try {
    $thannel->push(new SwowThannelBadValue());
} catch (Throwable $e) {
    echo get_class($e), ': ', $e->getMessage(), "\n";
}
?>
--EXPECT--
int(1)
bool(true)
full timeout
string(5) "world"
Exception: bad value
