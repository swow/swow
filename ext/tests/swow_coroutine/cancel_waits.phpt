--TEST--
swow_coroutine: cancelled waits release resources and run deferred cleanup once
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
use Swow\Socket;

use function Swow\defer;

foreach (['socket', 'channel', 'sleep'] as $wait) {
    foreach (['throw', 'kill'] as $operation) {
        $server = (new Socket(Socket::TYPE_TCP))->bind('127.0.0.1', 0)->listen();
        $client = (new Socket(Socket::TYPE_TCP))->setTimeout(1000)->connect('127.0.0.1', $server->getSockPort());
        $connection = $server->accept()->setTimeout(1000);
        $channel = new Channel(1);
        $exception = new RuntimeException('cancel ' . $wait);
        $caught = null;
        $continued = false;
        $cleanups = 0;
        $finallyCalls = 0;
        $resourceReference = null;
        $baseline = Coroutine::count();
        $worker = Coroutine::run(static function () use ($wait, $connection, $channel, &$caught, &$continued, &$cleanups, &$finallyCalls, &$resourceReference): void {
            $resource = new stdClass();
            $resourceReference = WeakReference::create($resource);
            defer(static function () use ($connection, &$cleanups): void {
                $cleanups++;
                $connection->close();
            });
            try {
                switch ($wait) {
                    case 'socket':
                        $connection->recvString(1);
                        break;
                    case 'channel':
                        $channel->pop(1000);
                        break;
                    case 'sleep':
                        usleep(1000 * 1000);
                        break;
                }
                $continued = true;
            } catch (Throwable $throwable) {
                $caught = $throwable;
            } finally {
                $finallyCalls++;
            }
        });
        try {
            Assert::true($worker->isAlive());
            Assert::notNull($resourceReference->get());
            if ($operation === 'throw') {
                $worker->throw($exception);
                if ($wait === 'sleep') {
                    Assert::same($caught, $exception);
                } else {
                    // Cancelled native operations add their own error context and preserve the injected cause.
                    Assert::same($caught->getPrevious(), $exception);
                    Assert::same($caught->getCode(), Errno::ECANCELED);
                }
            } else {
                $worker->kill();
                // kill() uses PHP's unwind exit, which bypasses both catch and finally.
                Assert::null($caught);
            }
            Assert::false($continued);
            Assert::false($worker->isAlive());
            Assert::same($cleanups, 1);
            Assert::same($finallyCalls, $operation === 'throw' ? 1 : 0);
            Assert::same(Coroutine::count(), $baseline);
            Assert::null($resourceReference->get());
            Assert::false($connection->isAvailable());
            Assert::same($client->recvString(1), '');
            // A cancelled channel wait must not retain a consumer or consume later messages.
            $channel->push('after cancellation', 1000);
            Assert::same($channel->pop(1000), 'after cancellation');
        } finally {
            if ($worker->isAlive()) {
                $worker->kill();
            }
            $connection->close();
            $client->close();
            $server->close();
        }
    }
}

echo "Done\n";
?>
--EXPECT--
Done
