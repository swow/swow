--TEST--
swow_event: fork for worker
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

    // receive work
    $lenWork = $conn->readString(4);
    $work = $conn->readString(unpack('N', $lenWork)[1]);
    $workload = unserialize($work);
    // do work
    $result = $workload['work'](...$workload['args']);
    // send result
    $result = serialize($result);
    $resultLen = pack('N', strlen($result));
    $conn->send($resultLen . $result);
} else {
    // parent
    printf("Parent PID: %d\n", getmypid());
    // close server connection
    $serverSock->close();
    // create new connection
    $clientSock = new Socket(Socket::TYPE_TCP);
    $conn = $clientSock->connect('127.0.0.1', $port);
    // send work
    $work = serialize([
        'work' => static function (int $a, int $b): int {
            printf("Calculate at pid %d\n", getmypid());
            return $a * $b;
        },
        'args' => [6, 7],
    ]);
    $conn->send(pack('N', strlen($work)) . $work);
    $resultLen = $conn->readString(4);
    $result = unserialize($conn->readString(unpack('N', $resultLen)[1]));
    printf("The answer to life, the universe, and everything is %d\n", $result);
    // receive result
    Assert::same($result, 42);
    printf("Done\n");
}
?>
--EXPECTF--
%s PID: %d
%s PID: %d
Calculate at pid %d
The answer to life, the universe, and everything is 42
Done
