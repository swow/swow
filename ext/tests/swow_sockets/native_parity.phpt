--TEST--
swow_sockets: ext/sockets gives the same results as native PHP
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
skip_if_win();
skip_if_extension_not_exist('sockets');
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

/** The result of a call, its error code and its warning, so that failures compare too */
function call(callable $function): array
{
    error_clear_last();
    socket_clear_error();
    $result = @$function();
    return [$result, socket_last_error(), error_get_last()['message'] ?? null];
}

function pair(int $type): array
{
    Assert::true(socket_create_pair(AF_UNIX, $type, 0, $pair));
    return $pair;
}

function probe(): array
{
    $results = [];

    // a local pair, as a process talks to its helper
    [$a, $b] = pair(SOCK_STREAM);
    $results['write'] = call(static fn() => socket_write($a, "hello\nline two\r\nrest"));
    $results['read line'] = call(static fn() => socket_read($b, 100, PHP_NORMAL_READ));
    $results['read line cr'] = call(static fn() => socket_read($b, 100, PHP_NORMAL_READ));
    $results['read line lf after cr'] = call(static fn() => socket_read($b, 100, PHP_NORMAL_READ));
    $results['read binary'] = call(static fn() => socket_read($b, 2));
    $results['read rest'] = call(static fn() => socket_read($b, 100));
    $results['write length'] = call(static fn() => socket_write($a, 'abcdef', 3));
    $results['write empty'] = call(static fn() => socket_write($a, ''));
    $results['send'] = call(static fn() => socket_send($a, 'xyz', 10, 0));
    $results['recv peek'] = call(static function () use ($b) {
        $r = socket_recv($b, $data, 2, MSG_PEEK);
        return [$r, $data];
    });
    $results['recv waitall'] = call(static function () use ($b) {
        $r = socket_recv($b, $data, 6, MSG_WAITALL);
        return [$r, $data];
    });

    // a read timeout of the socket
    socket_set_option($b, SOL_SOCKET, SO_RCVTIMEO, ['sec' => 0, 'usec' => 100000]);
    $results['read timeout'] = call(static fn() => socket_read($b, 10));
    $results['recv timeout'] = call(static function () use ($b) {
        $r = socket_recv($b, $data, 10, 0);
        return [$r, $data];
    });
    /*
     * MSG_WAITALL that times out after a part of the data: Linux returns that part, macOS
     * fails with EAGAIN and leaves it in the socket. Swow gathers the data as it arrives, as
     * Linux does, so this case is compared on Linux only.
     */
    if (PHP_OS_FAMILY === 'Linux') {
        $results['recv waitall timeout'] = call(static function () use ($a, $b) {
            socket_write($a, 'ab');
            $r = socket_recv($b, $data, 10, MSG_WAITALL);
            return [$r, $data];
        });
    }

    // a non-blocking socket without data
    socket_set_nonblock($b);
    $results['non-blocking read'] = call(static fn() => socket_read($b, 10));
    // (PHP_NORMAL_READ on a non-blocking socket without data spins forever, natively as well)
    $results['dontwait recv'] = call(static function () use ($a) {
        $r = socket_recv($a, $data, 10, MSG_DONTWAIT);
        return [$r, $data];
    });
    socket_set_block($b);

    // the peer goes away
    socket_close($a);
    $results['read eof'] = call(static fn() => socket_read($b, 10));
    $results['recv eof'] = call(static function () use ($b) {
        $r = socket_recv($b, $data, 10, 0);
        return [$r, $data];
    });
    $results['write closed'] = call(static fn() => socket_write($b, 'x'));
    socket_close($b);

    // TCP on the loopback
    $server = socket_create(AF_INET, SOCK_STREAM, SOL_TCP);
    socket_bind($server, '127.0.0.1', 0);
    socket_listen($server);
    socket_getsockname($server, $address, $port);
    socket_set_nonblock($server);
    $results['accept nothing'] = call(static fn() => socket_accept($server));
    socket_set_block($server);
    $client = socket_create(AF_INET, SOCK_STREAM, SOL_TCP);
    $results['connect'] = call(static fn() => socket_connect($client, '127.0.0.1', $port));
    $results['accept'] = call(static fn() => socket_accept($server) instanceof Socket);
    // a second connection, for a select on several sockets
    $second = socket_create(AF_INET, SOCK_STREAM, SOL_TCP);
    socket_connect($second, '127.0.0.1', $port);
    $accepted = socket_accept($server);
    socket_write($second, 'ping');
    // let the data arrive, so that both sockets are readable when select() looks
    usleep(100 * 1000);
    $results['select'] = call(static function () use ($accepted, $client) {
        $read = ['first' => $client, 'second' => $accepted];
        $write = $except = null;
        $n = socket_select($read, $write, $except, 1);
        return [$n, array_keys($read)];
    });
    $results['select timeout'] = call(static function () use ($client) {
        $read = [$client];
        $write = $except = null;
        return [socket_select($read, $write, $except, 0, 100000), array_keys($read)];
    });
    $results['select writable'] = call(static function () use ($client) {
        $read = $except = null;
        $write = [$client];
        return [socket_select($read, $write, $except, null), count($write)];
    });
    $results['blocking accepted'] = (static function () use ($accepted) {
        // the accepted socket is in blocking mode
        socket_set_option($accepted, SOL_SOCKET, SO_RCVTIMEO, ['sec' => 0, 'usec' => 50000]);
        socket_read($accepted, 4);
        return [socket_read($accepted, 4), socket_last_error($accepted)];
    })();
    socket_close($client);
    socket_close($second);
    socket_close($accepted);

    // connecting to a port without a listener
    socket_close($server);
    $refused = socket_create(AF_INET, SOCK_STREAM, SOL_TCP);
    $results['connect refused'] = call(static fn() => socket_connect($refused, '127.0.0.1', $port));

    // calls that fail at once natively must not wait for an event that never comes
    $idle = socket_create(AF_INET, SOCK_STREAM, SOL_TCP);
    $results['accept not listening'] = call(static fn() => socket_accept($idle));
    $results['read not connected'] = call(static fn() => socket_read($idle, 10));
    $results['recv not connected'] = call(static fn() => socket_recv($idle, $data, 10, 0));
    $results['write not connected'] = call(static fn() => socket_write($idle, 'x'));
    $results['recvfrom not connected'] = call(static fn() => socket_recvfrom($idle, $data, 10, 0, $from, $fromPort));
    socket_close($idle);

    // UDP datagrams
    $udp = socket_create(AF_INET, SOCK_DGRAM, SOL_UDP);
    socket_bind($udp, '127.0.0.1', 0);
    socket_getsockname($udp, $address, $udpPort);
    $sender = socket_create(AF_INET, SOCK_DGRAM, SOL_UDP);
    $results['sendto'] = call(static fn() => socket_sendto($sender, 'datagram', 8, 0, '127.0.0.1', $udpPort));
    $results['recvfrom'] = call(static function () use ($udp) {
        $r = socket_recvfrom($udp, $data, 100, 0, $from, $fromPort);
        return [$r, $data, $from];
    });
    socket_set_option($udp, SOL_SOCKET, SO_RCVTIMEO, ['sec' => 0, 'usec' => 100000]);
    $results['recvfrom timeout'] = call(static function () use ($udp) {
        $r = socket_recvfrom($udp, $data, 100, 0, $from, $fromPort);
        return [$r, $data];
    });

    // messages
    [$a, $b] = pair(SOCK_DGRAM);
    $results['sendmsg'] = call(static fn() => socket_sendmsg($a, ['iov' => ['mes', 'sage']], 0));
    $results['recvmsg'] = call(static function () use ($b) {
        $message = ['buffer_size' => 100];
        $r = socket_recvmsg($b, $message, 0);
        return [$r, $message['iov']];
    });

    return $results;
}

if (NativeBaseline::isNative()) {
    NativeBaseline::emit(probe());
}

$native = NativeBaseline::run(__FILE__, ['sockets']);
$swow = probe();
Assert::same($swow, $native, "native=\n" . json_encode($native, JSON_PRETTY_PRINT) . "\nswow=\n" . json_encode($swow, JSON_PRETTY_PRINT));
// the case compared on Linux only is left out of the expected output
echo json_encode(array_values(array_diff(array_keys($swow), ['recv waitall timeout']))), "\n";
?>
--EXPECT--
["write","read line","read line cr","read line lf after cr","read binary","read rest","write length","write empty","send","recv peek","recv waitall","read timeout","recv timeout","non-blocking read","dontwait recv","read eof","recv eof","write closed","accept nothing","connect","accept","select","select timeout","select writable","blocking accepted","connect refused","accept not listening","read not connected","recv not connected","write not connected","recvfrom not connected","sendto","recvfrom","recvfrom timeout","sendmsg","recvmsg"]
