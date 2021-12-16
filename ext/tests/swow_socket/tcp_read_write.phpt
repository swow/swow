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

class TestMethod
{
    private static string $testString;

    public static function init()
    {
        static::$testString = getRandomBytes();
    }

    public const SEND_TYPE_START = self::SEND;

    public const SEND = 1;

    public const SENDTO = 2;

    public const SENDSTRING = 3;

    public const SENDSTRINGTO = 4;

    public const WRITE = 5;

    public const WRITETO = 6;

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

    private int $offset;

    /**
     * @throws Exception
     */
    public function __construct()
    {
        $this->offset = random_int(0, strlen(static::$testString) - 1);
    }

    /**
     * @throws Exception
     */
    public static function parseType(int $type): string
    {
        switch ($type) {
            case 1:
                return 'send';
            case 2:
                return 'sendTo';
            case 3:
                return 'sendString';
            case 4:
                return 'sendStringTo';
            case 5:
                return 'write';
            case 6:
                return 'writeTo';
            case -1:
                return 'recv';
            case -2:
                return 'recvFrom';
            case -3:
                return 'recvData';
            case -4:
                return 'recvDataFrom';
            case -5:
                return 'recvString';
            case -6:
                return 'recvStringFrom';
            case -7:
                return 'recvStringData';
            case -8:
                return 'recvStringDataFrom';
            case -9:
                return 'read';
            case -10:
                return 'readString';
            case -11:
                return 'peek';
            case -12:
                return 'peekFrom';
            case -13:
                return 'peekString';
            case -14:
                return 'peekStringFrom';
            default:
                throw new Exception("unknown type {$type}");
        }
    }

    /**
     * @throws Exception
     */
    public function send(Socket $socket, int $type)
    {
        $testBuffer = new Buffer(strlen(static::$testString));
        $testBuffer->write(static::$testString);
        $testBuffer->seek($this->offset, SEEK_SET);
        switch ($type) {
            case static::SEND:
                $socket->send($testBuffer, -1, static::WRITE_TIMEOUT);
                break;
            case static::SENDTO:
                $socket->sendTo($testBuffer, -1, $socket->getPeerAddress(), $socket->getPeerPort(), static::WRITE_TIMEOUT);
                break;
            case static::WRITE:
                $socket->write(
                    [
                        [
                            /* buffer */ $testBuffer,
                            /* length */ -1,
                        ],
                    ],
                    static::WRITE_TIMEOUT
                );
                break;
            case static::WRITETO:
                $socket->writeTo(
                    [
                        [
                            /* buffer */ $testBuffer,
                            /* length */ -1,
                        ],
                    ],
                    $socket->getPeerAddress(),
                    $socket->getPeerPort(),
                    static::WRITE_TIMEOUT
                );
                break;
            case static::SENDSTRING:
                $socket->sendString(substr(static::$testString, $this->offset), static::WRITE_TIMEOUT);
                break;
            case static::SENDSTRINGTO:
                $socket->sendStringTo(substr(static::$testString, $this->offset), $socket->getPeerAddress(), $socket->getPeerPort(), static::WRITE_TIMEOUT);
                break;
        }
    }

    /**
     * @throws Exception
     */
    public function recv(Socket $socket, int $type)
    {
        $testBuffer = new Buffer(strlen(static::$testString));
        $length = strlen(static::$testString) - $this->offset;
        $timeout = static::READ_TIMEOUT;
        $received = 0;
        $received_str = null;
        switch ($type) {
            case static::RECV:
                $received = $socket->recv($testBuffer, -1, $timeout);
                break;
            case static::RECVFROM:
                $received = $socket->recvFrom($testBuffer, -1, $addr_from, $port_from, $timeout);
                break;
            case static::RECVDATA:
                $received = $socket->recvData($testBuffer, -1, $timeout);
                break;
            case static::RECVDATAFROM:
                $received = $socket->recvDataFrom($testBuffer, -1, $addr_from, $port_from, $timeout);
                break;
            case static::RECVSTRING:
                $received_str = $socket->recvString(Buffer::DEFAULT_SIZE, $timeout);
                $received = strlen($received_str);
                break;
            case static::RECVSTRINGFROM:
                $received_str = $socket->recvStringFrom(Buffer::DEFAULT_SIZE, $addr_from, $port_from, $timeout);
                $received = strlen($received_str);
                break;
            case static::RECVSTRINGDATA:
                $received_str = $socket->recvStringData(Buffer::DEFAULT_SIZE, $timeout);
                $received = strlen($received_str);
                break;
            case static::RECVSTRINGDATAFROM:
                $received_str = $socket->recvStringDataFrom(Buffer::DEFAULT_SIZE, $addr_from, $port_from, $timeout);
                $received = strlen($received_str);
                break;
            case static::READ:
                $received = $socket->read($testBuffer, $length, $timeout);
                break;
            case static::READSTRING:
                $received_str = $socket->readString($length, $timeout);
                $received = strlen($received_str);
                break;
            case static::PEEK:
                while ($timeout-- > 0) {
                    $received += $socket->peek($testBuffer);
                    if ($received < $length) {
                        msleep(1);
                    } else {
                        break;
                    }
                }
                // clean receiver buffer
                $socket->recv($testBuffer);
                break;
            case static::PEEKFROM:
                while ($timeout-- > 0) {
                    $received += $socket->peekFrom($testBuffer, -1, $addr_from, $port_from);
                    if ($received < $length) {
                        msleep(1);
                    } else {
                        break;
                    }
                }
                // clean receiver buffer
                $socket->recv($testBuffer);
                break;
            case static::PEEKSTRING:
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
            case static::PEEKSTRINGFROM:
                $received_str = '';
                while ($timeout-- > 0) {
                    $received_str .= $socket->peekStringFrom(Buffer::DEFAULT_SIZE, $addr_from, $port_from);
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
            Assert::same(substr(static::$testString, $this->offset), $received_str);
        } else {
            Assert::same(substr(static::$testString, $this->offset), $testBuffer->rewind()->read());
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
server peekStringFrom, client sendString
server peekStringFrom, client sendStringTo
server peekStringFrom, client write
server peekStringFrom, client writeTo
server peekString, client send
server peekString, client sendTo
server peekString, client sendString
server peekString, client sendStringTo
server peekString, client write
server peekString, client writeTo
server peekFrom, client send
server peekFrom, client sendTo
server peekFrom, client sendString
server peekFrom, client sendStringTo
server peekFrom, client write
server peekFrom, client writeTo
server peek, client send
server peek, client sendTo
server peek, client sendString
server peek, client sendStringTo
server peek, client write
server peek, client writeTo
server readString, client send
server readString, client sendTo
server readString, client sendString
server readString, client sendStringTo
server readString, client write
server readString, client writeTo
server read, client send
server read, client sendTo
server read, client sendString
server read, client sendStringTo
server read, client write
server read, client writeTo
server recvStringDataFrom, client send
server recvStringDataFrom, client sendTo
server recvStringDataFrom, client sendString
server recvStringDataFrom, client sendStringTo
server recvStringDataFrom, client write
server recvStringDataFrom, client writeTo
server recvStringData, client send
server recvStringData, client sendTo
server recvStringData, client sendString
server recvStringData, client sendStringTo
server recvStringData, client write
server recvStringData, client writeTo
server recvStringFrom, client send
server recvStringFrom, client sendTo
server recvStringFrom, client sendString
server recvStringFrom, client sendStringTo
server recvStringFrom, client write
server recvStringFrom, client writeTo
server recvString, client send
server recvString, client sendTo
server recvString, client sendString
server recvString, client sendStringTo
server recvString, client write
server recvString, client writeTo
server recvDataFrom, client send
server recvDataFrom, client sendTo
server recvDataFrom, client sendString
server recvDataFrom, client sendStringTo
server recvDataFrom, client write
server recvDataFrom, client writeTo
server recvData, client send
server recvData, client sendTo
server recvData, client sendString
server recvData, client sendStringTo
server recvData, client write
server recvData, client writeTo
server recvFrom, client send
server recvFrom, client sendTo
server recvFrom, client sendString
server recvFrom, client sendStringTo
server recvFrom, client write
server recvFrom, client writeTo
server recv, client send
server recv, client sendTo
server recv, client sendString
server recv, client sendStringTo
server recv, client write
server recv, client writeTo
client peekStringFrom, server send
client peekStringFrom, server sendTo
client peekStringFrom, server sendString
client peekStringFrom, server sendStringTo
client peekStringFrom, server write
client peekStringFrom, server writeTo
client peekString, server send
client peekString, server sendTo
client peekString, server sendString
client peekString, server sendStringTo
client peekString, server write
client peekString, server writeTo
client peekFrom, server send
client peekFrom, server sendTo
client peekFrom, server sendString
client peekFrom, server sendStringTo
client peekFrom, server write
client peekFrom, server writeTo
client peek, server send
client peek, server sendTo
client peek, server sendString
client peek, server sendStringTo
client peek, server write
client peek, server writeTo
client readString, server send
client readString, server sendTo
client readString, server sendString
client readString, server sendStringTo
client readString, server write
client readString, server writeTo
client read, server send
client read, server sendTo
client read, server sendString
client read, server sendStringTo
client read, server write
client read, server writeTo
client recvStringDataFrom, server send
client recvStringDataFrom, server sendTo
client recvStringDataFrom, server sendString
client recvStringDataFrom, server sendStringTo
client recvStringDataFrom, server write
client recvStringDataFrom, server writeTo
client recvStringData, server send
client recvStringData, server sendTo
client recvStringData, server sendString
client recvStringData, server sendStringTo
client recvStringData, server write
client recvStringData, server writeTo
client recvStringFrom, server send
client recvStringFrom, server sendTo
client recvStringFrom, server sendString
client recvStringFrom, server sendStringTo
client recvStringFrom, server write
client recvStringFrom, server writeTo
client recvString, server send
client recvString, server sendTo
client recvString, server sendString
client recvString, server sendStringTo
client recvString, server write
client recvString, server writeTo
client recvDataFrom, server send
client recvDataFrom, server sendTo
client recvDataFrom, server sendString
client recvDataFrom, server sendStringTo
client recvDataFrom, server write
client recvDataFrom, server writeTo
client recvData, server send
client recvData, server sendTo
client recvData, server sendString
client recvData, server sendStringTo
client recvData, server write
client recvData, server writeTo
client recvFrom, server send
client recvFrom, server sendTo
client recvFrom, server sendString
client recvFrom, server sendStringTo
client recvFrom, server write
client recvFrom, server writeTo
client recv, server send
client recv, server sendTo
client recv, server sendString
client recv, server sendStringTo
client recv, server write
client recv, server writeTo
Done
