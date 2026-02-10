--TEST--
swow_event: simple fork
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
skip_if_extension_not_exist('pcntl');
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Socket;

// create pipe
if (PHP_OS_FAMILY !== 'Windows') {
    define('SOCK_NAME', '/tmp/swow_flock_' . getRandomBytes(8) . '.sock');
} else {
    define('SOCK_NAME', '\\\?\pipe\swow_flock_' . getRandomBytes(8));
}

$serverSock = new Socket(Socket::TYPE_TCP);
$serverSock->bind('127.0.0.1', 0)->listen();
$port = $serverSock->getSockPort();

$pid = pcntl_fork();
if ($pid === -1) {
    // fork failed
    printf("Fork failed\n");
    exit(1);
}
if ($pid === 0) {
    // child
    printf("Child PID: %d\n", getmypid());
    $conn = $serverSock->accept(1000);
    $conn->send('hello');
} else {
    // parent
    printf("Parent PID: %d\n", getmypid());
    // close server connection
    $serverSock->close();
    // create new connection
    $clientSock = new Socket(Socket::TYPE_TCP);
    $conn = $clientSock->connect('127.0.0.1', $port);
    $msg = $conn->readString(5);
    Assert::same($msg, 'hello');
    printf("Done\n");
}
?>
--EXPECTF--
%s PID: %d
%s PID: %d
Done
