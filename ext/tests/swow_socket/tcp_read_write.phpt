--TEST--
swow_socket: tcp read write wrapper functions
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Buffer;
use Swow\Channel;
use Swow\Coroutine;
use Swow\Socket;
use Swow\Sync\WaitReference;

final class TestMethod
{
    private static string $testString;

    public static function init()
    {
        self::$testString = getRandomBytes();
    }

    public const SEND_TYPE_START = self::SEND;

    public const SEND = 1;

    public const SENDTO = 2;

    public const WRITE = 3;

    public const WRITETO = 4;

    public const SEND_TYPE_END = self::WRITETO;

    public const RECV_TYPE_END = self::RECV;

    public const RECV = -1;

    public const RECVFROM = -2;

    public const RECVDATA = -3;

    public const RECVDATAFROM = -4;

    public const RECVSTRING = -5;

    public const RECVSTRINGFROM = -6;

    public const RECVSTRINGDATA = -7;

    public const RECVSTRINGDATAFROM = -8;

    public const READ = -9;

    public const READSTRING = -10;

    public const PEEK = -11;

    public const PEEKFROM = -12;

    public const PEEKSTRING = -13;

    public const PEEKSTRINGFROM = -14;

    public const RECV_TYPE_START = self::PEEKSTRINGFROM;

    public const READ_TIMEOUT = 1001;

    public const WRITE_TIMEOUT = 1002;

    private int $start;

    /**
     * @throws Exception
     */
    public function __construct()
    {
        $this->start = random_int(0, strlen(self::$testString) - 1);
    }

    /**
     * @throws Exception
     */
    public static function parseType(int $type): string
    {
        return match ($type) {
            self::SEND => 'send',
            self::SENDTO => 'sendTo',
            self::WRITE => 'write',
            self::WRITETO => 'writeTo',
            self::RECV => 'recv',
            self::RECVFROM => 'recvFrom',
            self::RECVDATA => 'recvData',
            self::RECVDATAFROM => 'recvDataFrom',
            self::RECVSTRING => 'recvString',
            self::RECVSTRINGFROM => 'recvStringFrom',
            self::RECVSTRINGDATA => 'recvStringData',
            self::RECVSTRINGDATAFROM => 'recvStringDataFrom',
            self::READ => 'read',
            self::READSTRING => 'readString',
            self::PEEK => 'peek',
            self::PEEKFROM => 'peekFrom',
            self::PEEKSTRING => 'peekString',
            self::PEEKSTRINGFROM => 'peekStringFrom',
            default => throw new Exception('Unknown type'),
        };
    }

    /**
     * @throws Exception
     */
    public function send(Socket $socket, int $type)
    {
        $testBuffer = new Buffer(strlen(self::$testString));
        $testBuffer->append(self::$testString);
        switch ($type) {
            case self::SEND:
                $socket->send($testBuffer, $this->start, -1, self::WRITE_TIMEOUT);
                break;
            case self::SENDTO:
                $socket->sendTo($testBuffer, $this->start, -1, $socket->getPeerAddress(), $socket->getPeerPort(), self::WRITE_TIMEOUT);
                break;
            case self::WRITE:
                $socket->write([
                    [
                        /* buffer */ $testBuffer,
                        /* start */ $this->start,
                        /* length */ -1,
                    ],
                ], self::WRITE_TIMEOUT);
                break;
            case self::WRITETO:
                $socket->writeTo([
                    [
                        /* buffer */ $testBuffer,
                        /* start */ $this->start,
                        /* length */ -1,
                    ],
                ], $socket->getPeerAddress(), $socket->getPeerPort(), self::WRITE_TIMEOUT);
                break;
            default:
                throw new Exception('Unknown type');
        }
    }

    /**
     * @throws Exception
     */
    public function recv(Socket $socket, int $type)
    {
        $testBuffer = new Buffer(strlen(self::$testString));
        $length = strlen(self::$testString) - $this->start;
        $timeout = self::READ_TIMEOUT;
        $received = 0;
        $received_str = null;
        switch ($type) {
            case self::RECV:
                $received = $socket->recv($testBuffer, 0, -1, $timeout);
                break;
            case self::RECVFROM:
                $received = $socket->recvFrom($testBuffer, 0, -1, $addr_from, $port_from, $timeout);
                break;
            case self::RECVDATA:
                $received = $socket->recvData($testBuffer, 0, -1, $timeout);
                break;
            case self::RECVDATAFROM:
                $received = $socket->recvDataFrom($testBuffer, 0, -1, $addr_from, $port_from, $timeout);
                break;
            case self::RECVSTRING:
                $received_str = $socket->recvString(Buffer::COMMON_SIZE, $timeout);
                $received = strlen($received_str);
                break;
            case self::RECVSTRINGFROM:
                $received_str = $socket->recvStringFrom(Buffer::COMMON_SIZE, $addr_from, $port_from, $timeout);
                $received = strlen($received_str);
                break;
            case self::RECVSTRINGDATA:
                $received_str = $socket->recvStringData(Buffer::COMMON_SIZE, $timeout);
                $received = strlen($received_str);
                break;
            case self::RECVSTRINGDATAFROM:
                $received_str = $socket->recvStringDataFrom(Buffer::COMMON_SIZE, $addr_from, $port_from, $timeout);
                $received = strlen($received_str);
                break;
            case self::READ:
                $received = $socket->read($testBuffer, 0, $length, $timeout);
                break;
            case self::READSTRING:
                $received_str = $socket->readString($length, $timeout);
                $received = strlen($received_str);
                break;
            case self::PEEK:
                while ($timeout-- > 0) {
                    $received += $socket->peek($testBuffer, $received);
                    if ($received < $length) {
                        msleep(1);
                    } else {
                        break;
                    }
                }
                // clean receiver buffer
                $socket->recv(new Buffer($testBuffer->getSize()));
                break;
            case self::PEEKFROM:
                while ($timeout-- > 0) {
                    $received += $socket->peekFrom($testBuffer, $received, -1, $addr_from, $port_from);
                    if ($received < $length) {
                        msleep(1);
                    } else {
                        break;
                    }
                }
                // clean receiver buffer
                $socket->recv(new Buffer($testBuffer->getSize()));
                break;
            case self::PEEKSTRING:
                $received_str = '';
                while ($timeout-- > 0) {
                    $received_str .= $socket->peekString();
                    if (strlen($received_str) < $length) {
                        msleep(1);
                    } else {
                        break;
                    }
                }
                $received = strlen($received_str);
                // clean receiver buffer
                $socket->recv($testBuffer);
                break;
            case self::PEEKSTRINGFROM:
                $received_str = '';
                while ($timeout-- > 0) {
                    $received_str .= $socket->peekStringFrom(Buffer::COMMON_SIZE, $addr_from, $port_from);
                    if (strlen($received_str) < $length) {
                        msleep(1);
                    } else {
                        break;
                    }
                }
                $received = strlen($received_str);
                // clean receiver buffer
                $socket->recv($testBuffer);
                break;
        }

        Assert::same($received, $length);
        if ($received_str !== null) {
            Assert::same(substr(self::$testString, $this->start), $received_str);
        } else {
            Assert::same(substr(self::$testString, $this->start), (string) $testBuffer);
        }
    }
}

$wr = new WaitReference();

TestMethod::init();

$tester = new TestMethod();

$channel = new Channel(0);
$server = new Socket(Socket::TYPE_TCP);
$server->bind('127.0.0.1')->listen();
Coroutine::run(function () use ($channel, $tester, $server, $wr) {
    $connection = $server->accept();
    try {
        for ($type = TestMethod::RECV_TYPE_START; $type <= TestMethod::RECV_TYPE_END; $type++) {
            while (($remote_type = $channel->pop()) !== 'end round') {
                echo 'server ' . TestMethod::parseType($type) . ', client ' . TestMethod::parseType($remote_type) . PHP_LF;
                $tester->recv($connection, $type);
            }
        }
        for ($_ = TestMethod::RECV_TYPE_START; $_ <= TestMethod::RECV_TYPE_END; $_++) {
            for ($type = TestMethod::SEND_TYPE_START; $type <= TestMethod::SEND_TYPE_END; $type++) {
                $channel->push($type);
                $tester->send($connection, $type);
            }
            $channel->push('end round');
        }
    } finally {
        $connection->close();
        $channel->close();
    }
});

$client = new Socket(Socket::TYPE_TCP);
Coroutine::run(function () use ($tester, $channel, $server, $client, $wr) {
    $connection = $client->connect($server->getSockAddress(), $server->getSockPort());
    try {
        for ($_ = TestMethod::RECV_TYPE_START; $_ <= TestMethod::RECV_TYPE_END; $_++) {
            for ($type = TestMethod::SEND_TYPE_START; $type <= TestMethod::SEND_TYPE_END; $type++) {
                $channel->push($type);
                $tester->send($connection, $type);
            }
            $channel->push('end round');
        }
        for ($type = TestMethod::RECV_TYPE_START; $type <= TestMethod::RECV_TYPE_END; $type++) {
            while (($remote_type = $channel->pop()) !== 'end round') {
                echo 'client ' . TestMethod::parseType($type) . ', server ' . TestMethod::parseType($remote_type) . PHP_LF;
                $tester->recv($connection, $type);
            }
        }
    } finally {
        $connection->close();
        $channel->close();
    }
});

WaitReference::wait($wr);
// $server->close();
// $client->close();

echo 'Done' . PHP_LF;

?>
--EXPECT--
server peekStringFrom, client send
server peekStringFrom, client sendTo
server peekStringFrom, client write
server peekStringFrom, client writeTo
server peekString, client send
server peekString, client sendTo
server peekString, client write
server peekString, client writeTo
server peekFrom, client send
server peekFrom, client sendTo
server peekFrom, client write
server peekFrom, client writeTo
server peek, client send
server peek, client sendTo
server peek, client write
server peek, client writeTo
server readString, client send
server readString, client sendTo
server readString, client write
server readString, client writeTo
server read, client send
server read, client sendTo
server read, client write
server read, client writeTo
server recvStringDataFrom, client send
server recvStringDataFrom, client sendTo
server recvStringDataFrom, client write
server recvStringDataFrom, client writeTo
server recvStringData, client send
server recvStringData, client sendTo
server recvStringData, client write
server recvStringData, client writeTo
server recvStringFrom, client send
server recvStringFrom, client sendTo
server recvStringFrom, client write
server recvStringFrom, client writeTo
server recvString, client send
server recvString, client sendTo
server recvString, client write
server recvString, client writeTo
server recvDataFrom, client send
server recvDataFrom, client sendTo
server recvDataFrom, client write
server recvDataFrom, client writeTo
server recvData, client send
server recvData, client sendTo
server recvData, client write
server recvData, client writeTo
server recvFrom, client send
server recvFrom, client sendTo
server recvFrom, client write
server recvFrom, client writeTo
server recv, client send
server recv, client sendTo
server recv, client write
server recv, client writeTo
client peekStringFrom, server send
client peekStringFrom, server sendTo
client peekStringFrom, server write
client peekStringFrom, server writeTo
client peekString, server send
client peekString, server sendTo
client peekString, server write
client peekString, server writeTo
client peekFrom, server send
client peekFrom, server sendTo
client peekFrom, server write
client peekFrom, server writeTo
client peek, server send
client peek, server sendTo
client peek, server write
client peek, server writeTo
client readString, server send
client readString, server sendTo
client readString, server write
client readString, server writeTo
client read, server send
client read, server sendTo
client read, server write
client read, server writeTo
client recvStringDataFrom, server send
client recvStringDataFrom, server sendTo
client recvStringDataFrom, server write
client recvStringDataFrom, server writeTo
client recvStringData, server send
client recvStringData, server sendTo
client recvStringData, server write
client recvStringData, server writeTo
client recvStringFrom, server send
client recvStringFrom, server sendTo
client recvStringFrom, server write
client recvStringFrom, server writeTo
client recvString, server send
client recvString, server sendTo
client recvString, server write
client recvString, server writeTo
client recvDataFrom, server send
client recvDataFrom, server sendTo
client recvDataFrom, server write
client recvDataFrom, server writeTo
client recvData, server send
client recvData, server sendTo
client recvData, server write
client recvData, server writeTo
client recvFrom, server send
client recvFrom, server sendTo
client recvFrom, server write
client recvFrom, server writeTo
client recv, server send
client recv, server sendTo
client recv, server write
client recv, server writeTo
Done
