--TEST--
swow_debug: resources, labels, waiters and owners
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Channel;
use Swow\Coroutine;
use Swow\Socket;
use Swow\Sync\WaitReference;

use function Swow\Debug\getResources;
use function Swow\Debug\setResourceLabel;

function find_resource(string $id): array
{
    foreach (getResources() as $resource) {
        if ($resource['id'] === $id) {
            return $resource;
        }
    }
    throw new RuntimeException("{$id} is not listed");
}

$server = new Socket(Socket::TYPE_TCP);
$server->bind('127.0.0.1')->listen();
Assert::same($server->setLabel('http:listen')->getLabel(), 'http:listen');

$wr = new WaitReference();
$acceptor = Coroutine::run(static function () use ($server, $wr): void {
    $connection = $server->accept();
    $connection->recvString(4);
    $connection->close();
});
$listen = find_resource('socket:' . $server->getId());
Assert::same($listen['kind'], 'socket');
Assert::same($listen['label'], 'http:listen');
Assert::same($listen['state'], 'listening');
Assert::same($listen['waiters'], [$acceptor->getId()]);
Assert::same($acceptor->getWait()['op'], 'accept');

$client = new Socket(Socket::TYPE_TCP);
$client->connect($server->getSockAddress(), $server->getSockPort());
$client->send('ping');
$resource = find_resource('socket:' . $client->getId());
Assert::same($resource['peer'], $server->getSockAddress() . ':' . $server->getSockPort());
Assert::same($resource['bytesOut'], 4);
Assert::same($resource['owners'], [Coroutine::getCurrent()->getId()]);
setResourceLabel($resource['id'], 'upstream:test');
Assert::same($client->getLabel(), 'upstream:test');

$channel = new Channel();
$channel->setLabel('pool:test');
$pusher = Coroutine::run(static function () use ($channel): void {
    $channel->push(1);
});
$resource = find_resource('channel:' . $channel->getId());
Assert::same($resource['label'], 'pool:test');
Assert::same($resource['capacity'], 0);
Assert::same($resource['waiters'], [$pusher->getId()]);
setResourceLabel($resource['id'], null);
Assert::null($channel->getLabel());

Assert::throws(static fn() => setResourceLabel('channel:999999', 'x'), ValueError::class);
Assert::throws(static fn() => setResourceLabel('bogus', 'x'), ValueError::class);

$channel->pop();
/* the accepted connection goes away with the acceptor */
WaitReference::wait($wr);
$client->close();
$server->close();
foreach (getResources() as $resource) {
    Assert::notEq($resource['id'], 'socket:' . $server->getId());
}

echo json_encode(getResources()), "\n";
echo "Done\n";
?>
--EXPECTF--
[{"id":"channel:%d","kind":"channel","label":null,"capacity":0,"length":0,"closed":false,"waiters":[],"owners":[]}]
Done
