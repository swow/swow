--TEST--
swow_curl: base
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
skip_if_extension_not_exist('curl');
skip('only for cli', php_sapi_name() !== 'cli');
skip('extension must be built with libcurl', !Swow::isBuiltWith('curl'));
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Coroutine;
use Swow\Socket;
use Swow\SocketException;
use Swow\Sync\WaitReference;

// on-shot http server
$wrServer = new WaitReference();
$server = new Socket(Socket::TYPE_TCP);
Coroutine::run(function () use ($server, $wrServer) {
    $server->bind('127.0.0.1')->listen();
    while (true) {
        try {
            $connection = $server->accept();
        } catch (SocketException) {
            break;
        }
        Coroutine::run(function () use ($connection) {
            $request = '';
            $uri = '';
            $headers = [];
            do {
                @[$head, $body] = explode("\r\n\r\n", $request, 2);
                if ($body) {
                    [$firstLine, $headersString] = explode("\r\n", $head, 2);
                    $uri = explode(' ', $firstLine)[1];
                    $headers_lines = explode("\r\n", $headersString);
                    foreach ($headers_lines as $headers_line) {
                        [$k, $v] = explode(': ', $headers_line, 2);
                        $headers[strtolower($k)] = $v;
                    }
                    if (strlen($body) === (int) ($headers['content-length'] ?? 0)) {
                        Assert::same($headers['x-ceshi-header'], 'cafebabe');
                        break;
                    }
                }
            } while ($request .= $connection->recvString());
            // parse request
            $parsedUri = parse_url($uri);
            $query = $parsedUri['query'];
            parse_str($query, $parsedQuery);
            $name = $parsedQuery['name'] ?? 'foreigner';
            //var_dump($name);
            $requestPayload = json_decode($body, true);
            // build respond
            $payload = json_encode([
                    'code' => 0,
                    'msg' => "Hello {$name}",
                ] + $requestPayload);
            $response = [
                'HTTP/1.0 200 OK',
                'server: Swow/' . Swow::VERSION,
                'content-length: ' . strlen($payload),
                'content-type: application/json',
                'connection: close',
                '',
                $payload,
            ];
            $connection->sendString(implode("\r\n", $response));
            $connection->close();
        });
    }
});

// init ch

$chInit = function () use ($server) {
    $ch = curl_init();
    Assert::notSame($ch, false);
    Assert::same(curl_setopt_array($ch, [
        CURLOPT_URL => $server->getSockAddress() . ':' . $server->getSockPort() . '/?name=Swow',
        CURLOPT_HEADER => false,
        CURLOPT_HTTPHEADER => [
            'X-Ceshi-Header: cafebabe',
            'Content-Type: application/json',
        ],
        CURLOPT_POSTFIELDS => '{"age":2}',
        CURLOPT_AUTOREFERER => true,
        CURLOPT_FOLLOWLOCATION => true,
        CURLOPT_RETURNTRANSFER => true,
    ]), true);

    return $ch;
};

$chResponseVerify = function (string $response) {
    $ret = json_decode($response, true);
    Assert::same($ret['code'], 0);
    Assert::same($ret['msg'], 'Hello Swow');
    Assert::same($ret['age'], 2);
};

// curl it
$wrClient = new WaitReference();
for ($c = 0; $c < TEST_MAX_CONCURRENCY; $c++) {
    Coroutine::run(function () use ($server, $chInit, $chResponseVerify, $wrClient) {
        $ch = $chInit();
        $response = curl_exec($ch);
        $chResponseVerify($response);
        curl_close($ch);
    });
}
WaitReference::wait($wrClient);

// multi curl it
$mh = curl_multi_init();
$chs = [];
for ($c = 0; $c < TEST_MAX_CONCURRENCY; $c++) {
    $chs[] = $ch = $chInit();
    curl_multi_add_handle($mh, $ch);
}
do {
    $status = curl_multi_exec($mh, $active);
    if ($active) {
        curl_multi_select($mh);
    }
} while ($active && $status == CURLM_OK);
foreach ($chs as $ch) {
    $response = curl_multi_getcontent($ch);
    $chResponseVerify($response);
    curl_multi_remove_handle($mh, $ch);
    curl_close($ch);
}
curl_multi_close($mh);

$server->close();
WaitReference::wait($wrServer);

echo 'Done' . PHP_LF;
?>
--EXPECT--
Done
