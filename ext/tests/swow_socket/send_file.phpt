--TEST--
swow_socket: send file
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
skip_if_max_open_files_less_than(256);
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Coroutine;
use Swow\Errno;
use Swow\Socket;
use Swow\SocketException;
use Swow\Sync\WaitReference;

const FILE_SIZE = 64 * 1024 * 1024;
const RANDOM_SIZE = 1024;
const CHUNK_SIZE = 1024 * RANDOM_SIZE;
const CHUNK_COUNT = FILE_SIZE / CHUNK_SIZE;

$random = getRandomBytes(RANDOM_SIZE);
$randomChunk = str_repeat($random, CHUNK_SIZE / RANDOM_SIZE);
$tmpFile = tmpfile();
Assert::notSame($tmpFile, false);
for ($i = 0; $i < CHUNK_COUNT; $i++) {
    fwrite($tmpFile, $randomChunk);
}
Assert::same(fstat($tmpFile)['size'] ?? 0, FILE_SIZE);
$filename = stream_get_meta_data($tmpFile)['uri'];

$wrServer = new WaitReference();
$server = new Socket(Socket::TYPE_TCP);
Coroutine::run(static function () use ($server, $filename, $wrServer): void {
    $server->bind('127.0.0.1')->listen();
    try {
        while (true) {
            $connection = $server->accept();
            Coroutine::run(static function () use ($connection, $filename, $wrServer): void {
                Assert::same($connection->sendFile($filename), FILE_SIZE);
            });
        }
    } catch (SocketException $exception) {
        Assert::same($exception->getCode(), Errno::ECANCELED);
    }
});

$wrClient = new WaitReference();
for ($n = 0; $n < 1; $n++) {
    Coroutine::run(static function () use ($server, $random, $wrClient): void {
        $client = new Socket(Socket::TYPE_TCP);
        $client->connect($server->getSockAddress(), $server->getSockPort());
        for ($i = 0; $i < CHUNK_COUNT; $i++) {
            $receivedRandomChunk = $client->readString(CHUNK_SIZE);
            $receivedRandom = substr($receivedRandomChunk, $i * RANDOM_SIZE, RANDOM_SIZE);
            Assert::same($receivedRandom, $random);
        }
    });
}
WaitReference::wait($wrClient);
$server->close();
WaitReference::wait($wrServer);

echo "Done\n";

?>
--EXPECT--
Done
