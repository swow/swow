--TEST--
swow_stream: stream_socket_pair behaves as in native PHP
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
skip_if_win();
skip_if_function_not_exist('stream_socket_pair');
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

/** Everything here runs in one coroutine, so it must give the same results with and without Swow. */
function probe(): array
{
    $results = [];

    [$a, $b] = stream_socket_pair(STREAM_PF_UNIX, STREAM_SOCK_STREAM, STREAM_IPPROTO_IP);
    $results['stream_type'] = stream_get_meta_data($a)['stream_type'];
    fwrite($b, "line 1\nline 2\n");
    $results['fgets'] = [fgets($a), fgets($a)];

    // a blocking read without data waits for the read timeout
    stream_set_timeout($a, 0, 200 * 1000);
    $start = microtime(true);
    $results['read on timeout'] = fread($a, 10);
    $results['waited for the timeout'] = microtime(true) - $start >= 0.15;
    $results['timed_out'] = stream_get_meta_data($a)['timed_out'];
    $results['fgets on timeout'] = fgets($a);

    // a non-blocking read returns at once
    stream_set_blocking($a, false);
    $start = microtime(true);
    $results['non-blocking read'] = fread($a, 10);
    $results['returned at once'] = microtime(true) - $start < 0.1;
    stream_set_blocking($a, true);

    // the peer closes after writing
    fwrite($b, 'last');
    fclose($b);
    $results['read after peer closed'] = stream_get_contents($a);
    $results['feof'] = feof($a);
    fclose($a);

    // datagrams through the xport API
    [$a, $b] = stream_socket_pair(STREAM_PF_UNIX, STREAM_SOCK_DGRAM, STREAM_IPPROTO_IP);
    $results['sendto'] = [stream_socket_sendto($b, 'first'), stream_socket_sendto($b, 'second')];
    $results['recvfrom'] = [stream_socket_recvfrom($a, 100), stream_socket_recvfrom($a, 3, STREAM_PEEK), stream_socket_recvfrom($a, 100)];
    stream_set_timeout($a, 0, 100 * 1000);
    $results['dgram read on timeout'] = fread($a, 10);
    fclose($a);
    fclose($b);

    // writing more than the socket buffer holds, the reader is a child process here
    [$a, $b] = stream_socket_pair(STREAM_PF_UNIX, STREAM_SOCK_STREAM, STREAM_IPPROTO_IP);
    stream_set_blocking($b, false);
    $written = 0;
    while (($n = fwrite($b, str_repeat('x', 65536))) > 0) {
        $written += $n;
    }
    $results['non-blocking write fills the buffer'] = $written > 0 && $n === 0;
    fclose($a);
    fclose($b);

    return $results;
}

if (NativeBaseline::isNative()) {
    NativeBaseline::emit(probe());
}

$native = NativeBaseline::run(__FILE__);
$swow = probe();
// PHP 8.0 returns "" from a timed out read, later versions return false
Assert::same($swow, $native, 'native=' . json_encode($native) . ' swow=' . json_encode($swow));
echo json_encode($swow, JSON_PRETTY_PRINT), "\n";
?>
--EXPECTF--
{
    "stream_type": "generic_socket",
    "fgets": [
        "line 1\n",
        "line 2\n"
    ],
    "read on timeout": %s,
    "waited for the timeout": true,
    "timed_out": true,
    "fgets on timeout": false,
    "non-blocking read": "",
    "returned at once": true,
    "read after peer closed": "last",
    "feof": true,
    "sendto": [
        5,
        6
    ],
    "recvfrom": [
        "first",
        "sec",
        "second"
    ],
    "dgram read on timeout": %s,
    "non-blocking write fills the buffer": true
}
