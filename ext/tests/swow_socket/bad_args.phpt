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
}, Error::class);

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
            }, Error::class);
        }
        foreach ([
            'listen',
            'accept',
            'acceptTo',
            'getPeerAddress',
            'getPeerPort',
            'getSockAddress',
            'getSockPort',
            'recvString',
        ] as $function) {
            Assert::throws(function () use ($function) {
                $this->$function();
            }, Error::class);
        }
        Assert::throws(function () {
            $this->bind('127.0.0.1');
        }, Error::class);
        Assert::throws(function () {
            $this->connect('127.0.0.1', 1234);
        }, Error::class);
        Assert::throws(function () {
            $this->sendString('dasd');
        }, Error::class);
        Assert::throws(function () {
            $this->setTcpNodelay(false);
        }, Error::class);
        Assert::throws(function () {
            $this->setTcpAcceptBalance(false);
        }, Error::class);
        Assert::throws(function () {
            $this->setTcpKeepAlive(true, 1);
        }, Error::class);
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
    }, Error::class);
}
// cannot accept before binding
foreach ([
    'accept',
    'acceptTyped',
] as $function) {
    Assert::throws(function () use ($function, $socket) {
        $socket->$function();
    }, Error::class);
}

$server = new Socket(Socket::TYPE_TCP);

Assert::throws(function () use ($server) {
    // delay should be positive
    $server->setTcpKeepAlive(false, -234);
}, ValueError::class);

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
    }, ValueError::class);
}

// read length should greater than 0 or equal to -1
foreach ([
    'read',
    'recv',
    'recvFrom',
    'recvData',
    'recvDataFrom',
    'peek',
    'peekFrom',
] as $function) {
    Assert::throws(function () use ($buffer, $function, $conn) {
        $conn->$function($buffer, 0);
    }, ValueError::class);
    Assert::throws(function () use ($buffer, $function, $conn) {
        $conn->$function($buffer, -123);
    }, ValueError::class);
}

$buffer->write(str_repeat('cafebabe', 512));

// buffer writable length should greater than or equal to requested recv length
foreach ([
    'read',
    'recv',
    'recvFrom',
    'recvData',
    'recvDataFrom',
    'peek',
    'peekFrom',
] as $function) {
    Assert::throws(function () use ($buffer, $function, $conn) {
        $conn->$function($buffer, 12345);
    }, ValueError::class);
}

$buffer->rewind();

// send length should greater than or equal to -1
foreach ([
    'send',
    'sendTo',
] as $function) {
    $conn->$function($buffer, 0);
    Assert::throws(function () use ($buffer, $function, $conn) {
        $conn->$function($buffer, -123);
    }, ValueError::class);
}

// buffer readable length should greater than or equal to send length
foreach ([
    'send',
    'sendTo',
] as $function) {
    Assert::throws(function () use ($buffer, $function, $conn) {
        $conn->$function($buffer, 54321);
    }, ValueError::class);
}

// buffer readable length should greater than or equal to send length
Assert::throws(function () use ($buffer, $function, $conn) {
    $conn->sendString('a short string', 123, 0, 54321);
}, ValueError::class);
Assert::throws(function () use ($buffer, $function, $conn) {
    $conn->sendString('a short string', 123, 199, 2);
}, ValueError::class);
Assert::throws(function () use ($buffer, $function, $conn) {
    $conn->sendStringTo('a short string', '', 0, 123, 0, 54321);
}, ValueError::class);
Assert::throws(function () use ($buffer, $function, $conn) {
    $conn->sendStringTo('a short string', '', 0, 123, 199, 2);
}, ValueError::class);

// writev arguments checks
foreach (['write', 'writeTo'] as $function) {
    $notString = new class('im not a string-ish') {
        private $string;

        public function __construct($string)
        {
            $this->string = $string;
        }
    };
    Assert::throws(function () use ($function, $conn) {
        // empty buffers
        $conn->$function([]);
    }, ValueError::class);

    Assert::throws(function () use ($notString, $function, $conn) {
        // bad buffer spec
        $conn->$function([/* buffer spec */ $notString]);
    }, ValueError::class);

    Assert::throws(function () use ($notString, $function, $conn) {
        // bad buffer spec
        $conn->$function([[/* buffer-ish */ $notString]]);
    }, ValueError::class);

    Assert::throws(function () use ($buffer, $notString, $function, $conn) {
        // bad buffer spec
        $conn->$function([/* buffer spec */ []]);
    }, ValueError::class);

    Assert::throws(function () use ($buffer, $notString, $function, $conn) {
        // bad buffer spec
        $conn->$function([[/* buffer-ish: strange things */ []]]);
    }, ValueError::class);

    Assert::throws(function () use ($buffer, $notString, $function, $conn) {
        // bad buffer spec
        $conn->$function([[/* buffer-ish: buffer */ $buffer, /* send length */ 1, /* ?? */ 2, /* ?? */ 3]]);
    }, ValueError::class);

    Assert::throws(function () use ($buffer, $notString, $function, $conn) {
        // send buffer overflow
        $conn->$function([[/* buffer-ish: buffer */ $buffer, /* send length */ 99999]]);
    }, Throwable::class);

    Assert::throws(function () use ($buffer, $notString, $function, $conn) {
        // send buffer overflow
        $conn->$function([[/* buffer-ish: buffer */ $buffer, /* send length */ $notString]]);
    }, Throwable::class);

    Assert::throws(function () use ($buffer, $notString, $function, $conn) {
        // send buffer overflow
        $conn->$function([[/* buffer-ish: buffer */ $buffer, /* send length */ -123]]);
    }, Throwable::class);

    Assert::throws(function () use ($buffer, $notString, $function, $conn) {
        // send buffer overflow
        $conn->$function([[/* buffer-ish: string */ 'im a short string', /* offset */ 99999, /* send length */ 2]]);
    }, Throwable::class);

    Assert::throws(function () use ($buffer, $notString, $function, $conn) {
        // send buffer overflow
        $conn->$function([[/* buffer-ish: string */ 'im a short string', /* offset */ 0, /* send length */ 29999]]);
    }, Throwable::class);

    Assert::throws(function () use ($buffer, $notString, $function, $conn) {
        // send buffer overflow
        $conn->$function([[/* buffer-ish: string */ 'im a short string', /* offset */ $notString, /* send length */ 2]]);
    }, Throwable::class);

    Assert::throws(function () use ($buffer, $notString, $function, $conn) {
        // send buffer overflow
        $conn->$function([[/* buffer-ish: string */ 'im a short string', /* offset */ 0, /* send length */ $notString]]);
    }, Throwable::class);

    Assert::throws(function () use ($buffer, $notString, $function, $conn) {
        // send buffer overflow
        $conn->$function([[/* buffer-ish: string */ 'im a short string', /* offset */ -1, /* send length */ 2]]);
    }, Throwable::class);

    Assert::throws(function () use ($buffer, $notString, $function, $conn) {
        // send buffer overflow
        $conn->$function([[/* buffer-ish: string */ 'im a short string', /* offset */ 0, /* send length */ -123]]);
    }, Throwable::class);

    Assert::throws(function () use ($buffer, $notString, $function, $conn) {
        // bad buffer spec
        $conn->$function([[/* buffer-ish: buffer */ $buffer, /* send length */ 1, /* ?? */ 2]]);
    }, ValueError::class);
}

$conn->close();
$server->close();
$client->close();

echo 'Done' . PHP_LF;

?>
--EXPECT--
Done
