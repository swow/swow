--TEST--
swow_channel: getCapacity getLength
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Channel;
use Swow\Coroutine;

$bucket = new Channel(0);
Assert::same($bucket->getLength(), 0);
Assert::same($bucket->getCapacity(), 0);

$bucket = new Channel(2);
Assert::same($bucket->getLength(), 0);
Assert::same($bucket->getCapacity(), 2);

$bucket->push('some');
Assert::same($bucket->getLength(), 1);
Assert::same($bucket->getCapacity(), 2);

$bucket->push('data');
Assert::same($bucket->getLength(), 2);
Assert::same($bucket->getCapacity(), 2);

$bucket->pop();
Assert::same($bucket->getLength(), 1);
Assert::same($bucket->getCapacity(), 2);

$bucket->pop();
Assert::same($bucket->getLength(), 0);
Assert::same($bucket->getCapacity(), 2);


$fruits = ['apple', 'pear', 'orange', 'banana', 'grape'];

array_map(function ($fruit) use ($bucket) {
    Coroutine::run(function () use ($fruit, $bucket) {
        // "wash it"
        pseudo_random_sleep();
        $bucket->push("washed {$fruit}");
    });
}, $fruits);

for ($i = 0; $i < count($fruits); $i++) {
    $washed_fruit = $bucket->pop();
    [$washed, $fruit] = explode(' ', $washed_fruit);
    Assert::same($washed, 'washed');
    Assert::true(in_array($fruit, $fruits));
    Assert::same($bucket->getCapacity(), 2);
}

echo "Done\n";
?>
--EXPECT--
Done
