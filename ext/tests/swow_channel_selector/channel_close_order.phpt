--TEST--
swow_selector: closing a non final channel from the internal selector list should not crash php
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Channel;
use Swow\Coroutine;
use Swow\Errno;
use Swow\Selector;
use Swow\SelectorException;
use Swow\Sync\WaitGroup;

$selector = new Selector();
$dataChannel = new Channel(1);
$otherChannel = new Channel(1);
$readyChannel = new Channel(1);
$waitGroup = new WaitGroup();

$exceptionCode = null;

Coroutine::run(static function () use ($selector, $dataChannel, $otherChannel, $readyChannel, &$exceptionCode, $waitGroup): void {
    $waitGroup->add();
    try {
        $selector->pop($dataChannel);
        $selector->pop($otherChannel);

        $readyChannel->push(true);

        try {
            $selector->commit(1000);
            Assert::false('commit() unexpectedly succeeded');
        } catch (SelectorException $exception) {
            $exceptionCode = $exception->getCode();
        }
    } finally {
        $waitGroup->done();
    }
});

Coroutine::run(static function () use ($dataChannel, $readyChannel, $waitGroup): void {
    $waitGroup->add();
    try {
        $readyChannel->pop();
        usleep(10000);
        // close the first channel added to selector (any channel other than the last one will trigger this error)
        $dataChannel->close();
    } finally {
        $waitGroup->done();
    }
});

$waitGroup->wait();

Assert::same($exceptionCode, Errno::ECLOSED);

echo "Done\n";

?>
--EXPECT--
Done
