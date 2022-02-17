<?php
/**
 * This file is part of Swow
 *
 * @link    https://github.com/swow/swow
 * @contact twosee <twosee@php.net>
 *
 * For the full copyright and license information,
 * please view the LICENSE file that was distributed with this source code
 */

declare(strict_types=1);

use Swow\Coroutine;
use Swow\Socket;
use Swow\Sync\WaitReference;

define('C', 100);
define('N', 100);

$wr = new WaitReference();

$s = microtime(true);

// i just want to sleep...
for ($c = N; $c--;) {
    Coroutine::run(static function () use ($wr): void {
        for ($n = N; $n--;) {
            usleep(1000);
        }
    });
}

// 10K file read and write
for ($c = N; $c--;) {
    Coroutine::run(static function () use ($wr, $c): void {
        $tmp_filename = "/tmp/test-{$c}.php";
        $self = file_get_contents(__FILE__);
        if (!file_put_contents($tmp_filename, $self)) {
            return;
        }
        try {
            $wrSub = new WaitReference();
            for ($n = N; $n--;) {
                Coroutine::run(static function () use ($tmp_filename, $self, $wrSub): void {
                    assert(file_get_contents($tmp_filename) === $self);
                });
            }
            WaitReference::wait($wrSub);
        } finally {
            unlink($tmp_filename);
        }
    });
}

// 10K pdo and mysqli read
for ($c = C / 2; $c--;) {
    Coroutine::run(static function () use ($wr): void {
        $pdo = new PDO('mysql:host=127.0.0.1;dbname=mysql;charset=utf8', 'root', 'root');
        $statement = $pdo->prepare('SELECT `User` FROM `user`');
        for ($n = N; $n--;) {
            $statement->execute();
            assert(in_array('root', $statement->fetchAll(PDO::FETCH_COLUMN), true));
        }
    });
}
for ($c = C / 2; $c--;) {
    Coroutine::run(static function () use ($wr): void {
        $mysqli = new Mysqli('127.0.0.1', 'root', 'root', 'mysql');
        $statement = $mysqli->prepare('SELECT `User` FROM `user`');
        for ($n = N; $n--;) {
            $statement->bind_result($user);
            $statement->execute();
            while ($statement->fetch()) {
                if ($user === 'root') {
                    break;
                }
            }
            assert($user === 'root');
        }
    });
}

// php_stream tcp server & client with 12.8K requests in single process
function tcp_pack(string $data): string
{
    return pack('n', strlen($data)) . $data;
}

function tcp_length(string $head): int
{
    return unpack('n', $head)[1];
}

Coroutine::run(static function () use ($wr): void {
    $ctx = stream_context_create(['socket' => ['so_reuseaddr' => true, 'backlog' => C]]);
    $server = stream_socket_server(
        'tcp://0.0.0.0:9502',
        $errno,
        $errstr,
        STREAM_SERVER_BIND | STREAM_SERVER_LISTEN,
        $ctx
    );
    if (!$server) {
        echo "{$errstr} ({$errno})\n";
    } else {
        $c = 0;
        while ($conn = @stream_socket_accept($server, 1)) {
            Coroutine::run(static function () use ($wr, $server, $conn, &$c): void {
                stream_set_timeout($conn, 5);
                for ($n = N; $n--;) {
                    $data = fread($conn, tcp_length(fread($conn, 2)));
                    assert($data === "Hello Swow Server #{$n}");
                    fwrite($conn, tcp_pack("Hello Swow Client #{$n}"));
                }
                if (++$c === C) {
                    fclose($server);
                }
            });
        }
    }
});
for ($c = C; $c--;) {
    Coroutine::run(static function () use ($wr): void {
        $fp = stream_socket_client('tcp://127.0.0.1:9502', $errno, $errstr, 1);
        if (!$fp) {
            echo "{$errstr} ({$errno})\n";
        } else {
            stream_set_timeout($fp, 5);
            for ($n = N; $n--;) {
                fwrite($fp, tcp_pack("Hello Swow Server #{$n}"));
                $data = fread($fp, tcp_length(fread($fp, 2)));
                assert($data === "Hello Swow Client #{$n}");
            }
            fclose($fp);
        }
    });
}

// udp server & client with 12.8K requests in single process
Coroutine::run(static function () use ($wr): void {
    $socket = new Socket(Socket::TYPE_UDP);
    $socket->bind('127.0.0.1', 9503);
    $client_map = [];
    for ($c = C; $c--;) {
        for ($n = 0; $n < N; $n++) {
            $recv = $socket->recvStringFrom(32, $address, $port);
            $client_uid = "{$address}:{$port}";
            $id = $client_map[$client_uid] = ($client_map[$client_uid] ?? -1) + 1;
            assert($recv === "Client: Hello #{$id}");
            $socket->sendStringTo("Server: Hello #{$id}", $address, $port);
        }
    }
    $socket->close();
});
for ($c = C; $c--;) {
    Coroutine::run(static function () use ($wr): void {
        $fp = stream_socket_client('udp://127.0.0.1:9503', $errno, $errstr, 1);
        if (!$fp) {
            echo "{$errstr} ({$errno})\n";
        } else {
            for ($n = 0; $n < N; $n++) {
                fwrite($fp, "Client: Hello #{$n}");
                $recv = fread($fp, 1024);
                [$address, $port] = explode(':', (stream_socket_get_name($fp, true)));
                assert($address === '127.0.0.1' && ((int) $port) === 9503);
                assert($recv === "Server: Hello #{$n}");
            }
            fclose($fp);
        }
    });
}

WaitReference::wait($wr);

echo 'use ' . (microtime(true) - $s) . ' s';
