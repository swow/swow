--TEST--
swow_curl: file and string uploads preserve multipart content and metadata
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
skip_if(!Swow\Extension::isBuiltWith('curl'), 'extension must be built with libcurl');
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Coroutine;
use Swow\Socket;

$filename = tempnam(sys_get_temp_dir(), 'swow_curl_upload');
file_put_contents($filename, "file\0payload");
$server = new Socket(Socket::TYPE_TCP);
$server->bind('127.0.0.1')->listen();
$coroutine = null;
$handle = null;
$copies = [];
try {
    $file = new CURLFile($filename);
    Assert::same($file->getFilename(), $filename);
    Assert::same($file->getMimeType(), '');
    Assert::same($file->getPostFilename(), '');
    $file->setMimeType('application/octet-stream');
    $file->setPostFilename('file.bin');
    Assert::same($file->getMimeType(), 'application/octet-stream');
    Assert::same($file->getPostFilename(), 'file.bin');
    $created = curl_file_create($filename, 'text/plain', 'created.txt');
    Assert::same($created->getFilename(), $filename);
    Assert::same($created->getMimeType(), 'text/plain');
    Assert::same($created->getPostFilename(), 'created.txt');
    $stringFile = new CURLStringFile("string\0payload", 'string.bin');
    Assert::same($stringFile->mime, 'application/octet-stream');

    $coroutine = Coroutine::run(static function () use ($server): void {
        for ($transfer = 0; $transfer < 3; $transfer++) {
            $connection = $server->accept(1000);
            $connection->setTimeout(1000);
            try {
                $wire = '';
                while (!str_contains($wire, "\r\n\r\n")) {
                    $wire .= $connection->recvString();
                }
                [$headers, $body] = explode("\r\n\r\n", $wire, 2);
                Assert::true(preg_match('/\r\nContent-Length: (\d+)/i', $headers, $matches) === 1);
                $length = (int) $matches[1];
                while (strlen($body) < $length) {
                    $body .= $connection->recvString();
                }
                Assert::same(strlen($body), $length);
                Assert::contains($headers, 'POST /upload HTTP/1.1');
                Assert::contains($headers, 'multipart/form-data; boundary=');
                Assert::contains($body, "name=\"title\"\r\n\r\nSwow\r\n");
                Assert::contains($body, 'name="disk"; filename="file.bin"');
                Assert::contains($body, 'name="memory"; filename="string.bin"');
                Assert::contains($body, "file\0payload\r\n");
                Assert::contains($body, "string\0payload\r\n");
                $connection->send("HTTP/1.1 200 OK\r\nContent-Length: 4\r\nConnection: close\r\n\r\nDone");
            } finally {
                $connection->close();
            }
        }
    });
    $handle = curl_init('http://' . $server->getSockAddress() . ':' . $server->getSockPort() . '/upload');
    Assert::true(curl_setopt_array($handle, [
        CURLOPT_PROXY => '',
        CURLOPT_RETURNTRANSFER => true,
        CURLOPT_TIMEOUT_MS => 1000,
        CURLOPT_HTTPHEADER => ['Expect:'],
        CURLOPT_POSTFIELDS => ['title' => 'Swow', 'disk' => $file, 'memory' => $stringFile],
    ]));
    $copies = [clone $handle, curl_copy_handle($handle)];
    $fileReference = WeakReference::create($file);
    $stringReference = WeakReference::create($stringFile);
    unset($file, $created, $stringFile);
    Assert::same(curl_exec($handle), 'Done');
    Assert::same(curl_errno($handle), CURLE_OK);
    unset($handle);
    // Both duplication APIs rebuild multipart data and retain upload metadata after the original is released.
    Assert::notNull($fileReference->get());
    Assert::notNull($stringReference->get());
    foreach ($copies as $copy) {
        Assert::same(curl_exec($copy), 'Done');
        Assert::same(curl_errno($copy), CURLE_OK);
    }
    unset($copy, $copies);
    Assert::null($fileReference->get());
    Assert::null($stringReference->get());
    Assert::false($coroutine->isAlive());
} finally {
    unset($handle, $copy, $copies);
    $server->close();
    if ($coroutine?->isAlive()) {
        $coroutine->kill();
    }
    unlink($filename);
}

echo "Done\n";
?>
--EXPECT--
Done
