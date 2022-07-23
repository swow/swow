--TEST--
swow_zend: assign val to typed properties by ref
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

$waitReferenceAssign = static function (object $storage): bool {
    $storage->wr = new WaitReference();
    $storage->wr::wait($storage->wr);
    return true;
};
Assert::throws(fn() => $waitReferenceAssign(new class {
    public int $wr = 0;
}), TypeError::class);
Assert::throws(fn() => $waitReferenceAssign(new class {
    public string $wr = '';
}), TypeError::class);
Assert::true($waitReferenceAssign(new class {
    public ?WaitReference $wr = null;
}));

$readFromAssign = static function (object $storage): object {
    $channel = new Channel();
    $server = new Socket(Socket::TYPE_UDP);
    $server->bind('127.0.0.1', 0);
    Coroutine::run(function () use ($server, $storage, $channel) {
        try {
            Assert::same($server->recvStringFrom(1, $storage->address, $storage->port), 'X');
        } catch (Throwable $throwable) {
        }
        $channel->push($throwable ?? null);
    });
    $client = new Socket(Socket::TYPE_UDP);
    $client->sendStringTo('X', $server->getSockAddress(), $server->getSockPort());
    $throwable = $channel->pop();
    if ($throwable) {
        throw $throwable;
    }
    return $storage;
};
Assert::throws(fn() => $readFromAssign(new class {
    public int $address = 0;
    public string $port = '';
}), TypeError::class);
var_dump(get_object_vars($readFromAssign(new class {
    public string $address = '';
    public int $port = 0;
})));

?>
--EXPECTF--
array(2) {
  ["address"]=>
  string(9) "127.0.0.1"
  ["port"]=>
  int(%d)
}
