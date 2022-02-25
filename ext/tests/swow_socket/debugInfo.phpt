--TEST--
swow_socket: socket debuginfo
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Coroutine;
use Swow\Socket;
use Swow\Sync\WaitReference;

class MySocket extends Socket
{
    public function __construct(int $type = self::TYPE_TCP)
    {
        // not initialized
        var_dump($this);
        parent::__construct($type);
    }
}

$server = new MySocket(MySocket::TYPE_TCP);

// not connected
var_dump($server);

$wr = new WaitReference();

$server->bind('127.0.0.1')->listen();
// bound
var_dump($server);

$server_coro = Coroutine::run(function () use ($server, &$conn, $wr) {
    $conn = $server->accept();
});

$client = new Socket(Socket::TYPE_TCP);
$client_coro = Coroutine::run(function () use ($client, $server, $wr) {
    $client->connect($server->getSockAddress(), $server->getSockPort());
});

WaitReference::wait($wr);

// connected
var_dump($server);
var_dump($conn);
var_dump($client);

$server->close();
$client->close();

// closed
var_dump($server);

echo 'Done' . PHP_LF;

?>
--EXPECTF--
object(MySocket)#%d (%d) {
  ["type"]=>
  string(7) "UNKNOWN"
  ["fd"]=>
  int(-1)
}
object(MySocket)#%d (%d) {
  ["type"]=>
  string(3) "TCP"
  ["fd"]=>
  int(-1)
  ["timeout"]=>
  array(6) {
    ["dns"]=>
    int(%s)
    ["accept"]=>
    int(%s)
    ["connect"]=>
    int(%s)
    ["handshake"]=>
    int(%s)
    ["read"]=>
    int(%s)
    ["write"]=>
    int(%s)
  }
  ["established"]=>
  bool(false)
  ["role"]=>
  string(7) "unknown"
  ["sockname"]=>
  NULL
  ["peername"]=>
  NULL
  ["io_state"]=>
  string(4) "idle"
}
object(MySocket)#%d (%d) {
  ["type"]=>
  string(4) "TCP4"
  ["fd"]=>
  int(%d)
  ["timeout"]=>
  array(6) {
    ["dns"]=>
    int(%s)
    ["accept"]=>
    int(%s)
    ["connect"]=>
    int(%s)
    ["handshake"]=>
    int(%s)
    ["read"]=>
    int(%s)
    ["write"]=>
    int(%s)
  }
  ["established"]=>
  bool(false)
  ["role"]=>
  string(6) "server"
  ["sockname"]=>
  array(2) {
    ["address"]=>
    string(9) "127.0.0.1"
    ["port"]=>
    int(%d)
  }
  ["peername"]=>
  NULL
  ["io_state"]=>
  string(4) "idle"
}
object(MySocket)#%d (8) {
  ["type"]=>
  string(4) "TCP4"
  ["fd"]=>
  int(%d)
  ["timeout"]=>
  array(6) {
    ["dns"]=>
    int(%s)
    ["accept"]=>
    int(%s)
    ["connect"]=>
    int(%s)
    ["handshake"]=>
    int(%s)
    ["read"]=>
    int(%s)
    ["write"]=>
    int(%s)
  }
  ["established"]=>
  bool(false)
  ["role"]=>
  string(6) "server"
  ["sockname"]=>
  array(2) {
    ["address"]=>
    string(9) "127.0.0.1"
    ["port"]=>
    int(%d)
  }
  ["peername"]=>
  NULL
  ["io_state"]=>
  string(4) "idle"
}
object(MySocket)#%d (8) {
  ["type"]=>
  string(4) "TCP4"
  ["fd"]=>
  int(%d)
  ["timeout"]=>
  array(6) {
    ["dns"]=>
    int(%s)
    ["accept"]=>
    int(%s)
    ["connect"]=>
    int(%s)
    ["handshake"]=>
    int(%s)
    ["read"]=>
    int(%s)
    ["write"]=>
    int(%s)
  }
  ["established"]=>
  bool(true)
  ["role"]=>
  string(17) "server-connection"
  ["sockname"]=>
  array(2) {
    ["address"]=>
    string(9) "127.0.0.1"
    ["port"]=>
    int(%d)
  }
  ["peername"]=>
  array(2) {
    ["address"]=>
    string(9) "127.0.0.1"
    ["port"]=>
    int(%d)
  }
  ["io_state"]=>
  string(4) "idle"
}
object(Swow\Socket)#%d (8) {
  ["type"]=>
  string(4) "TCP4"
  ["fd"]=>
  int(%d)
  ["timeout"]=>
  array(6) {
    ["dns"]=>
    int(%s)
    ["accept"]=>
    int(%s)
    ["connect"]=>
    int(%s)
    ["handshake"]=>
    int(%s)
    ["read"]=>
    int(%s)
    ["write"]=>
    int(%s)
  }
  ["established"]=>
  bool(true)
  ["role"]=>
  string(6) "client"
  ["sockname"]=>
  array(2) {
    ["address"]=>
    string(9) "127.0.0.1"
    ["port"]=>
    int(%d)
  }
  ["peername"]=>
  array(2) {
    ["address"]=>
    string(9) "127.0.0.1"
    ["port"]=>
    int(%d)
  }
  ["io_state"]=>
  string(4) "idle"
}
object(MySocket)#%d (2) {
  ["type"]=>
  string(7) "UNKNOWN"
  ["fd"]=>
  int(-1)
}
Done
