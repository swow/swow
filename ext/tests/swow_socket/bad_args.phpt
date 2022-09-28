--TEST--
swow_socket: bad args passed in
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Buffer;
use Swow\Coroutine;
use Swow\Socket;
use Swow\Sync\WaitReference;

class MySocket1 extends Socket
{
    public function __construct(int $type = Socket::TYPE_TCP)
    {
        // can be constructed only once
        parent::__construct($type);
        parent::__construct($type);
    }
}
Assert::throws(function () {
    new MySocket1();
}, Error::class, expectMessage: '/MySocket1 can be constructed only once/');

class MySocket2 extends Socket
{
    public function __construct(int $type = Socket::TYPE_TCP)
    {
        // not constructed
        foreach ([
            'setTimeout',
            'setDnsTimeout',
            'setAcceptTimeout',
            'setConnectTimeout',
            'setHandshakeTimeout',
            'setReadTimeout',
            'setWriteTimeout',
            'setRecvBufferSize',
            'setSendBufferSize',
        ] as $function) {
            Assert::throws(function () use ($function) {
                $this->$function(-1);
            }, Error::class, expectMessage: '/Socket has not been constructed/');
        }
        foreach ([
            'listen',
            'accept',
            // 'acceptTo',
            'getPeerAddress',
            'getPeerPort',
            'getSockAddress',
            'getSockPort',
            'recvString',
        ] as $function) {
            Assert::throws(function () use ($function) {
                $this->$function();
            }, Error::class, expectMessage: '/Socket has not been constructed/');
        }
        Assert::throws(function () {
            $this->bind('127.0.0.1');
        }, Error::class, expectMessage: '/Socket has not been constructed/');
        Assert::throws(function () {
            $this->connect('127.0.0.1', 1234);
        }, Error::class, expectMessage: '/Socket has not been constructed/');
        Assert::throws(function () {
            $this->send('dasd');
        }, Error::class, expectMessage: '/Socket has not been constructed/');
        Assert::throws(function () {
            $this->setTcpNodelay(false);
        }, Error::class, expectMessage: '/Socket has not been constructed/');
        Assert::throws(function () {
            $this->setTcpAcceptBalance(false);
        }, Error::class, expectMessage: '/Socket has not been constructed/');
        Assert::throws(function () {
            $this->setTcpKeepAlive(true, 1);
        }, Error::class, expectMessage: '/Socket has not been constructed/');
        parent::__construct($type);
    }
}
$socket = new MySocket2();
$socket->close();

$socket = new Socket(Socket::TYPE_PIPE);
// local socket (pipe, unix socket, udg socket) should not have local port and remote port
foreach ([
    'getPeerPort',
    'getSockPort',
] as $function) {
    Assert::throws(function () use ($function, $socket) {
        $socket->$function();
    }, Error::class, expectMessage: '/Local socket has no port/');
}
// cannot accept before binding
foreach ([
    'accept',
    // 'acceptTo',
] as $function) {
    Assert::throws(function () use ($function, $socket) {
        $socket->$function();
    }, Error::class, expectMessage: '/Socket is not listening for connections/');
}

$server = new Socket(Socket::TYPE_TCP);

Assert::throws(function () use ($server) {
    // delay should be positive
    $server->setTcpKeepAlive(enable: false, delay: -234);
}, ValueError::class, expectMessage: '/Argument #\d \(\$delay\) must be greater than 0 and less than or equal to \d+/');

$client = new Socket(Socket::TYPE_TCP);

$server->bind('127.0.0.1')->listen();
$wr = new WaitReference();

Coroutine::run(function () use ($wr, $server, &$conn) {
    $conn = $server->accept();
});

Coroutine::run(function () use ($wr, $client, $server) {
    $client->connect($server->getSockAddress(), $server->getSockPort());
});

WaitReference::wait($wr);

$buffer = new Buffer(4096);

// read length should greater than 0
foreach ([
    'readString',
    'recvString',
    'recvStringData',
    'recvStringDataFrom',
    'peekString',
    'peekStringFrom',
] as $function) {
    Assert::throws(function () use ($function, $conn) {
        $conn->$function(-1);
    }, ValueError::class, expectMessage: '/Argument #\d \(\$\w+\) must be greater than 0/');
}

// read length should greater than 0 or equal to -1
// buffer writable length should greater than or equal to requested recv length
foreach ([
    'read',
] as $function) {
    Assert::throws(function () use ($buffer, $function, $conn) {
        $conn->$function($buffer, offset: -123);
    }, ValueError::class, expectMessage: '/Argument #\d \(\$offset\) can not be negative/');
    Assert::throws(function () use ($buffer, $function, $conn) {
        $conn->$function($buffer, offset: PHP_INT_MAX);
    }, ValueError::class, expectMessage: '/Argument #\d \(\$offset\) can not be greater than buffer length \(\d+\)/');
    Assert::throws(function () use ($buffer, $function, $conn) {
        $conn->$function($buffer, length: 0);
    }, ValueError::class, expectMessage: '/Argument #\d \(\$length\) can not be 0/');
    Assert::throws(function () use ($buffer, $function, $conn) {
        $conn->$function($buffer, length: -123);
    }, ValueError::class, expectMessage: '/Argument #\d \(\$length\) can only be -1 to refer to unlimited when it is negative/');
    Assert::throws(function () use ($buffer, $function, $conn) {
        $conn->$function($buffer, length: PHP_INT_MAX);
    }, ValueError::class, expectMessage: '/Argument #\d \(\$length\) with offset \(\d+\) can not be greater than buffer size \(\d+\)/');
}
foreach ([
    'recv',
    'recvFrom',
    'recvData',
    'recvDataFrom',
    'peek',
    'peekFrom',
] as $function) {
    Assert::throws(function () use ($buffer, $function, $conn) {
        $conn->$function($buffer, offset: -123);
    }, ValueError::class, expectMessage: '/Argument #\d \(\$offset\) can not be negative/');
    Assert::throws(function () use ($buffer, $function, $conn) {
        $conn->$function($buffer, offset: PHP_INT_MAX);
    }, ValueError::class, expectMessage: '/Argument #\d \(\$offset\) can not be greater than buffer length \(\d+\)/');
    Assert::throws(function () use ($buffer, $function, $conn) {
        $conn->$function($buffer, size: 0);
    }, ValueError::class, expectMessage: '/Argument #\d \(\$size\) can not be 0/');
    Assert::throws(function () use ($buffer, $function, $conn) {
        $conn->$function($buffer, size: -123);
    }, ValueError::class, expectMessage: '/Argument #\d \(\$size\) can only be -1 to refer to unlimited when it is negative/');
    Assert::throws(function () use ($buffer, $function, $conn) {
        $conn->$function($buffer, size: PHP_INT_MAX);
    }, ValueError::class, expectMessage: '/Argument #\d \(\$size\) with offset \(\d+\) can not be greater than buffer size \(\d+\)/');
}

$buffer->append(str_repeat('cafebabe', 512));

// send length should greater than or equal to -1
foreach ([
    'send',
    'sendTo',
] as $function) {
    $conn->$function($buffer, 0);
    Assert::throws(function () use ($buffer, $function, $conn) {
        $conn->$function($buffer, start: -123);
    }, ValueError::class, expectMessage: '/Argument #\d \(\$start\) can not be negative/');
    Assert::throws(function () use ($buffer, $function, $conn) {
        $conn->$function($buffer, length: -123);
    }, ValueError::class, expectMessage: '/Argument #\d \(\$length\) can only be -1 to refer to unlimited when it is negative/');
}

// buffer readable length should greater than or equal to send length
foreach ([
    'send',
    'sendTo',
] as $function) {
    Assert::throws(function () use ($buffer, $function, $conn) {
        $conn->$function($buffer, 54321);
    }, ValueError::class, expectMessage: '/Argument #\d \(\$start\) can not be greater than buffer length \(\d+\)/');
}

// buffer readable length should greater than or equal to send length
Assert::throws(function () use ($buffer, $function, $conn) {
    $conn->send('a short string', length: 54321);
}, ValueError::class, expectMessage: '/Argument #\d \(\$length\) with start \(\d+\) can not be greater than string length \(\d+\)/');
Assert::throws(function () use ($buffer, $function, $conn) {
    $conn->send('a short string', start: 199, length: 2);
}, ValueError::class, expectMessage: '/Argument #\d \(\$start\) can not be greater than string length \(\d+\)/');
Assert::throws(function () use ($buffer, $function, $conn) {
    $conn->sendTo('a short string', length: 54321);
}, ValueError::class, expectMessage: '/Argument #\d \(\$length\) with start \(\d+\) can not be greater than string length \(\d+\)/');
Assert::throws(function () use ($buffer, $function, $conn) {
    $conn->sendTo('a short string', start: 199, length: 2);
}, ValueError::class, expectMessage: '/Argument #\d \(\$start\) can not be greater than string length \(\d+\)/');

// writev arguments checks
foreach (['write', 'writeTo'] as $function) {
    $notString = new class('im not a string-ish') {
        public function __construct(private string $string) { }
    };
    $stringable = new class('stringable') {
        public function __construct(private string $string) { }
        public function __toString(): string
        {
            return $this->string;
        }
    };
    Assert::throws(function () use ($function, $conn) {
        // empty buffers
        $conn->$function([]);
    }, ValueError::class, expectMessage: '/Argument #1 \(\$vector\) can not be empty/');

    Assert::throws(function () use ($notString, $function, $conn) {
        // bad buffer spec
        $conn->$function([/* buffer spec */ $notString]);
    }, TypeError::class, expectMessage: '/Argument #1 \(\$vector\) \[0\] \(\$stringable\) must be of type string, array or [\w\\\\]+, class@anonymous given/');

    Assert::throws(function () use ($notString, $function, $conn) {
        // bad buffer spec
        $conn->$function([[/* buffer-ish */ $notString]]);
    }, TypeError::class, expectMessage: '/Argument #1 \(\$vector\) \[0\]\[0\] \(\$data\) must be of type string or [\w\\\\]+, class@anonymous given/');

    Assert::throws(function () use ($buffer, $function, $conn) {
        // bad buffer spec
        $conn->$function([/* buffer spec */ []]);
    }, ValueError::class, expectMessage: '/Argument #1 \(\$vector\) \[0\] must have 1 to 3 elements, 0 given/');

    Assert::throws(function () use ($buffer, $function, $conn) {
        // bad buffer spec
        $conn->$function([[/* buffer-ish: strange things */ []]]);
    }, TypeError::class, expectMessage: '/Argument #1 \(\$vector\) \[0\]\[0\] \(\$data\) must be of type string or [\w\\\\]+, array given/');

    Assert::throws(function () use ($buffer, $function, $conn) {
        // bad buffer spec
        $conn->$function([[/* buffer-ish: buffer */ $buffer, /* start */ 2, /* send length */ 3, /* ?? */ 4]]);
    }, ValueError::class, expectMessage: '/Argument #1 \(\$vector\) \[0\] must have 1 to 3 elements, 4 given/');

    Assert::throws(function () use ($buffer, $function, $conn) {
        // send buffer overflow
        $conn->$function([[/* buffer-ish: buffer */ $buffer, /* start */ 0, /* send length */ 99999]]);
    }, ValueError::class, expectMessage: '/Argument #1 \(\$vector\) \[0\]\[2\] \(\$length = 99999\) with start \(0\) can not be greater than buffer length \(4096\)/');

    Assert::throws(function () use ($buffer, $notString, $function, $conn) {
        // send buffer overflow
        $conn->$function([[/* buffer-ish: buffer */ $buffer, /* start */ 0, /* send length */ $notString]]);
    }, TypeError::class, expectMessage: '/Argument #1 \(\$vector\) \[0\]\[2\] \(\$length\) must be of type int, class@anonymous given/');

    Assert::throws(function () use ($buffer, $function, $conn) {
        // send buffer overflow
        $conn->$function([[/* buffer-ish: buffer */ $buffer, /* start */ -1, /* send length */ 2]]);
    }, Throwable::class, expectMessage: '/Argument #1 \(\$vector\) \[0\]\[1\] \(\$start = -1\) can not be negative/');

    Assert::throws(function () use ($buffer, $function, $conn) {
        // send buffer overflow
        $conn->$function([[/* buffer-ish: buffer */ $buffer, /* start */ 0, /* send length */ -123]]);
    }, ValueError::class, expectMessage: '/Argument #1 \(\$vector\) \[0\]\[2\] \(\$length = -123\) can only be -1 to refer to unlimited when it is negative/');

    Assert::throws(function () use ($function, $conn) {
        // send buffer overflow
        $conn->$function([[/* buffer-ish: string */ 'im a short string', /* start */ 99999, /* send length */ 2]]);
    }, Throwable::class, expectMessage: '/Argument #1 \(\$vector\) \[0\]\[1\] \(\$start = 99999\) can not be greater than string length \(\d+\)/');

    Assert::throws(function () use ($function, $conn) {
        // send buffer overflow
        $conn->$function([[/* buffer-ish: string */ 'im a short string', /* start */ 0, /* send length */ 29999]]);
    }, Throwable::class, expectMessage: '/Argument #1 \(\$vector\) \[0\]\[2\] \(\$length = 29999\) with start \(0\) can not be greater than string length \(\d+\)/');

    Assert::throws(function () use ($notString, $function, $conn) {
        // send buffer overflow
        $conn->$function([[/* buffer-ish: string */ 'im a short string', /* start */ $notString, /* send length */ 2]]);
    }, Throwable::class, expectMessage: '/Argument #1 \(\$vector\) \[0\]\[1\] \(\$start\) must be of type int, class@anonymous given/');

    Assert::throws(function () use ($notString, $function, $conn) {
        // send buffer overflow
        $conn->$function([[/* buffer-ish: string */ 'im a short string', /* start */ 0, /* send length */ $notString]]);
    }, Throwable::class, expectMessage: '/Argument #1 \(\$vector\) \[0\]\[2\] \(\$length\) must be of type int, class@anonymous given/');

    Assert::throws(function () use ($function, $conn) {
        // send buffer overflow
        $conn->$function([[/* buffer-ish: string */ 'im a short string', /* start */ -1, /* send length */ 2]]);
    }, Throwable::class, expectMessage: '/Argument #1 \(\$vector\) \[0\]\[1\] \(\$start = -1\) can not be negative/');

    Assert::throws(function () use ($function, $conn) {
        // send buffer overflow
        $conn->$function([[/* buffer-ish: string */ 'im a short string', /* start */ 0, /* send length */ -123]]);
    }, ValueError::class, expectMessage: '/Argument #1 \(\$vector\) \[0\]\[2\] \(\$length = -123\) can only be -1 to refer to unlimited when it is negative/');

    Assert::throws(function () use ($buffer, $function, $conn) {
        // bad buffer spec
        $conn->$function([[/* buffer-ish: buffer */ $buffer, /* start */ 0, /* send length */ 1, /* ?? */ 2]]);
    }, ValueError::class, expectMessage: '/Argument #1 \(\$vector\) \[0\] must have 1 to 3 elements, 4 given/');
}

$conn->close();
$server->close();
$client->close();

echo "Done\n";

?>
--EXPECT--
Done
