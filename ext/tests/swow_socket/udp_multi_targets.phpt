--TEST--
swow_socket: udp multi targets
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Buffer;
use Swow\Coroutine;
use Swow\Errno;
use Swow\Socket;
use Swow\SocketException;
use Swow\Sync\WaitReference;

Socket::setGlobalReadTimeout(1000);

function testMultiClient(string $name, callable $function): void
{
    echo "{$name} start\n";
    $wr = new WaitReference();

    $server = new Socket(Socket::TYPE_UDP);
    $server->bind('127.0.0.1');

    $count = 0;

    Coroutine::run(static function () use (&$count, $server, $function, $wr): void {
        for ($i = 0; $i < TEST_MAX_REQUESTS; $i++) {
            try {
                [$data, $peer_port] = $function($server);
            } catch (SocketException $e) {
                Assert::same($e->getCode(), Errno::ETIMEDOUT);
                return;
            }
            [1 => $peer_port_in_data, 2 => $local_port_in_data] = unpack('N2', $data);
            Assert::same($peer_port, $peer_port_in_data);
            Assert::same($server->getSockPort(), $local_port_in_data);
            $count++;
        }
    });

    for ($i = 0; $i < TEST_MAX_REQUESTS; $i++) {
        Coroutine::run(static function () use ($server, $wr): void {
            $client = new Socket(Socket::TYPE_UDP);
            $client->bind('127.0.0.1');
            pseudo_random_sleep();
            $client->sendTo(pack('N2', $client->getSockPort(), $server->getSockPort()), address: $server->getSockAddress(), port: $server->getSockPort());
            $client->close();
        });
    }

    WaitReference::wait($wr);
    $server->close();

    Assert::greaterThan($count, 0);
    echo "{$name} verified {$count} times\n";
}

testMultiClient('recvFrom', static function (Socket $server) {
    $buffer = new Buffer(8192);
    $server->recvFrom($buffer, address: $addr_from, port: $port_from);

    return [$buffer->toString(), $port_from];
});

testMultiClient('peekFrom', static function (Socket $server) {
    $buffer = new Buffer(8192);
    while (8 > $server->peekFrom($buffer, size: 8, address: $addr_from, port: $port_from)) {
        msleep(1);
    }

    return [$buffer->toString(), $port_from];
});

testMultiClient('peekFrom(with-timeout)', static function (Socket $server) {
    $buffer = new Buffer(8192);
    while (true) {
        if (8 <= $server->peekFrom($buffer, size: 8, address: $addr_from, port: $port_from, timeout: -1)) {
            break;
        }
    }
    return [$buffer->toString(), $port_from];
});

testMultiClient('recvDataFrom', static function (Socket $server) {
    $buffer = new Buffer(8192);
    $server->recvDataFrom($buffer, address: $addr_from, port: $port_from);

    return [$buffer->toString(), $port_from];
});

testMultiClient('recvStringFrom', static function (Socket $server) {
    $ret = $server->recvStringFrom(8192, $addr_from, $port_from);

    return [$ret, $port_from];
});

testMultiClient('peekStringFrom', static function (Socket $server) {
    while (($ret = $server->peekStringFrom(8192, $addr_from, $port_from)) === '') {
        msleep(1);
    }

    return [$ret, $port_from];
});

testMultiClient('recvStringDataFrom', static function (Socket $server) {
    $ret = $server->recvStringDataFrom(8192, $addr_from, $port_from);

    return [$ret, $port_from];
});

function testMultiServer(string $name, callable $function): void
{
    echo "{$name} start\n";

    $wr = new WaitReference();

    $count = 0;
    // start multi servers
    $servers = [];
    for ($i = 0; $i < TEST_MAX_REQUESTS; $i++) {
        $server = $servers[] = new Socket(Socket::TYPE_UDP);
        Coroutine::run(static function () use (&$count, $server, $wr): void {
            $server->bind('127.0.0.1');
            try {
                $data = $server->recvStringDataFrom(8192, $_, $peer_port);
                [1 => $peer_port_in_data, 2 => $local_port_in_data] = unpack('N2', $data);
                Assert::same($peer_port, $peer_port_in_data);
                Assert::same($server->getSockPort(), $local_port_in_data);
                $count++;
            } catch (SocketException $e) {
                // do nothing
                Assert::same($e->getCode(), Errno::ETIMEDOUT);
            }
        });
    }

    $client = new Socket(Socket::TYPE_UDP);
    $client->bind('127.0.0.1');
    foreach ($servers as $server) {
        $function($client, $server);
    }
    $client->close();

    WaitReference::wait($wr);
    foreach ($servers as $server) {
        $server->close();
    }
    Assert::greaterThan($count, 0);
    echo "{$name} verified {$count} times\n";
}

testMultiServer('sendTo(string)', static function (Socket $client, Socket $server): void {
    $client->sendTo(pack('N2', $client->getSockPort(), $server->getSockPort()), address: $server->getSockAddress(), port: $server->getSockPort());
});

testMultiServer('writeTo', static function (Socket $client, Socket $server): void {
    $client->writeTo([
        [
            /* buffer */ pack('N', $client->getSockPort()),
            /* offset */ 0,
            /* length */ 4,
        ],
        [
            /* buffer */ pack('N', $server->getSockPort()),
            /* offset */ 0,
            /* length */ 4,
        ],
    ], $server->getSockAddress(), $server->getSockPort());
});

testMultiServer('sendTo(buffer)', static function (Socket $client, Socket $server): void {
    $buffer = new Buffer(8192);
    $buffer->append(pack('N', $client->getSockPort()));
    $buffer->append(pack('N', $server->getSockPort()));
    $client->sendTo($buffer, address: $server->getSockAddress(), port: $server->getSockPort());
});

echo "Done\n";
?>
--EXPECTF--
recvFrom start
recvFrom verified %d times
peekFrom start
peekFrom verified %d times
peekFrom(with-timeout) start
peekFrom(with-timeout) verified 128 times
recvDataFrom start
recvDataFrom verified %d times
recvStringFrom start
recvStringFrom verified %d times
peekStringFrom start
peekStringFrom verified %d times
recvStringDataFrom start
recvStringDataFrom verified %d times
sendTo(string) start
sendTo(string) verified %d times
writeTo start
writeTo verified %d times
sendTo(buffer) start
sendTo(buffer) verified %d times
Done
