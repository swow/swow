--TEST--
swow_sockets: blocking calls of ext/sockets let other coroutines run
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
skip_if_win();
skip_if_extension_not_exist('sockets');
skip_if_extension_not_exist('pcntl');
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Coroutine;
use Swow\Sync\WaitReference;

function listener(): array
{
    $server = socket_create(AF_INET, SOCK_STREAM, SOL_TCP);
    socket_set_option($server, SOL_SOCKET, SO_REUSEADDR, 1);
    Assert::true(socket_bind($server, '127.0.0.1', 0));
    Assert::true(socket_listen($server, 128));
    socket_getsockname($server, $address, $port);
    return [$server, $port];
}

function connectTo(int $port): Socket
{
    $client = socket_create(AF_INET, SOCK_STREAM, SOL_TCP);
    Assert::true(socket_connect($client, '127.0.0.1', $port));
    return $client;
}

/**
 * Answer each line of a connection with the line in upper case and the name of the server.
 * A line read at the end of the stream fails with ECONNRESET, as it does natively.
 */
function serve(Socket $connection, string $name): void
{
    while (($line = @socket_read($connection, 1024, PHP_NORMAL_READ)) !== false && $line !== '') {
        if (trim($line) === '') {
            continue;
        }
        socket_write($connection, strtoupper(trim($line)) . " by {$name}\n");
    }
    socket_close($connection);
}

/*
 * Prefork: the parent listens, a forked worker and the parent accept on the same socket.
 * When the other process takes a connection first, the accept of this process must keep
 * waiting on the event loop; a blocking accept() would stop this process, and the client
 * coroutines below with it.
 */
[$server, $port] = listener();
$worker = pcntl_fork();
if ($worker === 0) {
    // the parent kills the worker; should the parent fail first, the worker ends by itself
    Coroutine::run(static function (): void {
        sleep(30);
        Swow\Process\kill(getmypid(), SIGKILL);
    });
    for (;;) {
        $connection = socket_accept($server);
        if ($connection !== false) {
            Coroutine::run('serve', $connection, 'worker');
        }
    }
}
$acceptor = Coroutine::run(static function () use ($server): void {
    while (($connection = @socket_accept($server)) !== false) {
        Coroutine::run('serve', $connection, 'parent');
    }
});
$served = [];
$measured = SchedulerProbe::measure(static function () use ($port, &$served): void {
    $wr = new WaitReference();
    for ($client = 0; $client < 4; $client++) {
        Coroutine::run(static function () use ($client, $port, $wr, &$served): void {
            for ($request = 0; $request < 5; $request++) {
                $connection = connectTo($port);
                socket_write($connection, "client {$client} request {$request}\n");
                $reply = socket_read($connection, 1024, PHP_NORMAL_READ);
                Assert::startsWith($reply, "CLIENT {$client} REQUEST {$request} by ");
                $served[] = trim(substr($reply, strrpos($reply, ' ')));
                socket_close($connection);
                usleep(5 * 1000);
            }
        });
    }
    WaitReference::wait($wr);
});
Assert::same(count($served), 20);
Assert::greaterThan($measured['ticks'], 0, 'the ticker must run while the processes accept');
Swow\Process\kill($worker, SIGKILL);
Assert::same(pcntl_waitpid($worker, $status), $worker);
$acceptor->kill();
socket_close($server);
echo "prefork: 20 requests served\n";

// a server coroutine and its clients in one process, with line reads
[$server, $port] = listener();
$acceptor = Coroutine::run(static function () use ($server): void {
    while (($connection = @socket_accept($server)) !== false) {
        Coroutine::run('serve', $connection, 'server');
    }
});
$replies = [];
$wr = new WaitReference();
for ($client = 0; $client < 4; $client++) {
    Coroutine::run(static function () use ($client, $port, $wr, &$replies): void {
        $connection = connectTo($port);
        for ($line = 0; $line < 5; $line++) {
            // a line in two pieces: the server reads a part of it and waits for the rest
            socket_write($connection, "client {$client} ");
            usleep(2 * 1000);
            socket_write($connection, "line {$line}\n");
            $replies[$client][] = trim(socket_read($connection, 1024, PHP_NORMAL_READ));
        }
        socket_close($connection);
    });
}
WaitReference::wait($wr);
ksort($replies);
foreach ($replies as $client => $lines) {
    Assert::same($lines, array_map(static fn (int $line): string => "CLIENT {$client} LINE {$line} by server", range(0, 4)));
}
echo "line server: ", count($replies), " clients\n";

// a payload far larger than the socket buffers between two coroutines of one thread
Assert::true(socket_create_pair(AF_UNIX, SOCK_STREAM, 0, $pair));
[$a, $b] = $pair;
$payload = random_bytes(4 * 1024 * 1024);
$received = null;
$reader = Coroutine::run(static function () use ($b, $payload, &$received): void {
    Assert::same(socket_recv($b, $received, strlen($payload), MSG_WAITALL), strlen($payload));
});
Assert::same(socket_write($a, $payload), strlen($payload));
while ($reader->isAlive()) {
    usleep(1000);
}
Assert::same(md5($received), md5($payload));
echo "payload: ok\n";

// two coroutines reading one socket get one message each
$messages = [];
$wr = new WaitReference();
for ($i = 0; $i < 2; $i++) {
    Coroutine::run(static function () use ($b, $wr, &$messages): void {
        $messages[] = socket_read($b, 5);
    });
}
socket_write($a, 'first');
usleep(20 * 1000);
socket_write($a, 'secnd');
WaitReference::wait($wr);
sort($messages);
Assert::same($messages, ['first', 'secnd']);
echo "two readers: ok\n";

// every blocking call lets the other coroutines run while it waits
$wait = 200;
$later = static function (callable $action) use ($wait): void {
    Coroutine::run(static function () use ($action, $wait): void {
        usleep($wait * 1000);
        $action();
    });
};
$later(static fn () => socket_write($a, "data\n"));
SchedulerProbe::assertYields(static fn () => Assert::same(socket_read($b, 100), "data\n"), $wait, 'socket_read()');
$later(static fn () => socket_write($a, "line\n"));
SchedulerProbe::assertYields(static fn () => Assert::same(socket_read($b, 100, PHP_NORMAL_READ), "line\n"), $wait, 'socket_read() of a line');
$later(static fn () => socket_send($a, 'abcd', 4, 0));
SchedulerProbe::assertYields(static function () use ($b): void {
    Assert::same(socket_recv($b, $data, 4, 0), 4);
}, $wait, 'socket_recv()');
$later(static function () use ($a): void {
    socket_write($a, 'ab');
    usleep(50 * 1000);
    socket_write($a, 'cd');
});
SchedulerProbe::assertYields(static function () use ($b): void {
    Assert::same(socket_recv($b, $data, 4, MSG_WAITALL), 4);
    Assert::same($data, 'abcd');
}, $wait, 'socket_recv() with MSG_WAITALL');
$later(static fn () => socket_write($a, 'ready'));
SchedulerProbe::assertYields(static function () use ($b): void {
    $read = [$b];
    $write = $except = null;
    Assert::same(socket_select($read, $write, $except, null), 1);
    Assert::same(socket_read($b, 5), 'ready');
}, $wait, 'socket_select() without a timeout');
SchedulerProbe::assertYields(static function () use ($b): void {
    $read = [$b];
    $write = $except = null;
    Assert::same(socket_select($read, $write, $except, 0, 300 * 1000), 0);
    Assert::same($read, []);
}, 300, 'socket_select() until its timeout');

socket_set_option($b, SOL_SOCKET, SO_RCVTIMEO, ['sec' => 0, 'usec' => 300 * 1000]);
SchedulerProbe::assertYields(static function () use ($b): void {
    Assert::false(@socket_read($b, 10));
    Assert::same(socket_last_error($b), SOCKET_EAGAIN);
}, 300, 'socket_read() until SO_RCVTIMEO');
SchedulerProbe::assertYields(static function () use ($b): void {
    Assert::same(socket_recv($b, $data, 10, MSG_WAITALL), false);
}, 300, 'socket_recv() with MSG_WAITALL until SO_RCVTIMEO');
socket_close($a);
socket_close($b);

[$server, $port] = listener();
$later(static fn () => socket_close(connectTo($port)));
SchedulerProbe::assertYields(static fn () => Assert::isInstanceOf(socket_accept($server), Socket::class), $wait, 'socket_accept()');
socket_close($server);

$udp = socket_create(AF_INET, SOCK_DGRAM, SOL_UDP);
socket_bind($udp, '127.0.0.1', 0);
socket_getsockname($udp, $address, $udpPort);
$later(static function () use ($udpPort): void {
    $sender = socket_create(AF_INET, SOCK_DGRAM, SOL_UDP);
    socket_sendto($sender, 'datagram', 8, 0, '127.0.0.1', $udpPort);
});
SchedulerProbe::assertYields(static function () use ($udp): void {
    Assert::same(socket_recvfrom($udp, $data, 100, 0, $from, $fromPort), 8);
    Assert::same($data, 'datagram');
}, $wait, 'socket_recvfrom()');
echo "every blocking call yields\n";
?>
--EXPECT--
prefork: 20 requests served
line server: 4 clients
payload: ok
two readers: ok
every blocking call yields
