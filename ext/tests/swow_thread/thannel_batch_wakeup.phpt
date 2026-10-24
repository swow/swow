--TEST--
swow_thannel: every consumer wakes up when the queue holds a message for it
--SKIPIF--
<?php
if (!PHP_ZTS) {
    exit('skip ZTS only');
}
require __DIR__ . '/../include/skipif.php';
?>
--INI--
swow.closure_serializer=1
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Thannel;
use Swow\Thread;

$thannel = new Thannel(10);
$consumers = [
    Thread::run([$thannel, 'pop'], 1500),
    Thread::run([$thannel, 'pop'], 1500),
];
usleep(200000);

// a batch has to wake one consumer per message, or the other one stays parked
// next to a queue that still holds a value
Assert::same(2, $thannel->pushMany(['a', 'b']));
$values = [$consumers[0]->wait(), $consumers[1]->wait()];
sort($values);
Assert::same(['a', 'b'], $values);
Assert::same(0, $thannel->getLength());

echo "done\n";
?>
--EXPECT--
done
