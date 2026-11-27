--TEST--
swow_stream: stream_socket_pair connects coroutines without blocking the scheduler
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
skip_if_function_not_exist('stream_socket_pair');
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Coroutine;
use Swow\Sync\WaitReference;

$domain = PHP_OS_FAMILY === 'Windows' ? STREAM_PF_INET : STREAM_PF_UNIX;

/*
 * In-process messaging: several producer coroutines write length-prefixed
 * messages into one end, a consumer reads them from the other end. Before the
 * fix the first blocking read stopped the whole thread, and the producers never
 * ran, which is a deadlock.
 */
[$consumerEnd, $producerEnd] = stream_socket_pair($domain, STREAM_SOCK_STREAM, STREAM_IPPROTO_IP);
$producers = 4;
$messagesPerProducer = 20;
$wr = new WaitReference();
for ($id = 0; $id < $producers; $id++) {
    Coroutine::run(static function () use ($id, $producerEnd, $messagesPerProducer, $wr): void {
        for ($n = 0; $n < $messagesPerProducer; $n++) {
            $message = "producer {$id} message {$n}";
            fwrite($producerEnd, pack('N', strlen($message)) . $message);
            usleep(1000);
        }
    });
}
$received = [];
$measured = SchedulerProbe::measure(static function () use ($consumerEnd, $producers, $messagesPerProducer, &$received): void {
    while (count($received) < $producers * $messagesPerProducer) {
        $length = unpack('N', fread($consumerEnd, 4))[1];
        $received[] = fread($consumerEnd, $length);
    }
});
WaitReference::wait($wr);
Assert::same(count($received), $producers * $messagesPerProducer);
Assert::same(count(array_unique($received)), count($received));
Assert::greaterThan($measured['ticks'], 0, 'the ticker must run while the consumer waits');
echo "messages: ", count($received), "\n";

/*
 * A payload far larger than the socket buffer: the writer fills the buffer and
 * has to wait for the reader, who runs in another coroutine of the same thread.
 */
$payload = random_bytes(4 * 1024 * 1024);
$digest = null;
$reader = Coroutine::run(static function () use ($consumerEnd, $payload, &$digest): void {
    $data = '';
    while (strlen($data) < strlen($payload)) {
        $data .= fread($consumerEnd, 65536);
    }
    $digest = md5($data);
});
Assert::same(fwrite($producerEnd, $payload), strlen($payload));
while ($reader->isAlive()) {
    usleep(1000);
}
Assert::same($digest, md5($payload));
echo "payload: ok\n";

// a reader waiting for its read timeout lets the other coroutines run
stream_set_timeout($consumerEnd, 0, 300 * 1000);
$data = SchedulerProbe::assertYields(static fn () => fread($consumerEnd, 1), 300, 'fread() waiting for its timeout');
Assert::true($data === '' || $data === false, 'a timed out read returns "" (PHP 8.0) or false');
Assert::true(stream_get_meta_data($consumerEnd)['timed_out']);
echo "timeout: ok\n";

// a peer that answers late, as a worker coroutine replying to a request
Coroutine::run(static function () use ($producerEnd): void {
    usleep(200 * 1000);
    fwrite($producerEnd, "reply\n");
});
stream_set_timeout($consumerEnd, 5);
$line = SchedulerProbe::assertYields(static fn () => fgets($consumerEnd), 200, 'fgets() waiting for a late reply');
echo 'reply: ', $line;

// the peer goes away while a reader waits
Coroutine::run(static function () use ($producerEnd): void {
    usleep(200 * 1000);
    fclose($producerEnd);
});
$line = SchedulerProbe::assertYields(static fn () => fgets($consumerEnd), 200, 'fgets() waiting for the peer to close');
var_dump($line, feof($consumerEnd));
?>
--EXPECT--
messages: 80
payload: ok
timeout: ok
reply: reply
bool(false)
bool(true)
