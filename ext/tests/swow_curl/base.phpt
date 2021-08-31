--TEST--
swow_curl: base
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
skip_if_extension_not_exist('curl');
skip('only for cli', php_sapi_name() !== 'cli');
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Coroutine;
use Swow\Socket;

// on-shot http server
$server = new Socket(Socket::TYPE_TCP);
Coroutine::run(function () use ($server) {
    $server->bind('127.0.0.1')->listen();
    $conn = $server->accept();
    $req = '';
    do {
        @[$head, $body] = explode("\r\n\r\n", $req, 2);
        if ($body) {
            [$first_line, $headers_str] = explode("\r\n", $head, 2);
            [$verb, $uri, $http_ver] = explode(' ', $first_line);
            $headers_lines = explode("\r\n", $headers_str);
            $headers = [];
            foreach ($headers_lines as $headers_line) {
                [$k, $v] = explode(': ', $headers_line, 2);
                $headers[strtolower($k)] = $v;
            }
            if (strlen($body) === (int) ($headers['content-length'] ?? 0)) {
                break;
            }
        }
    } while ($req .= $conn->recvString());
    //var_dump($verb, $uri, $http_ver, $headers, $body);
    // parse request
    $parsed_uri = parse_url($uri);
    $query = $parsed_uri['query'];
    parse_str($query, $parsed_query);
    $name = $parsed_query['name'] ?? 'foreigner';
    //var_dump($name);
    $req_payload = json_decode($body, true);
    // build respond
    $payload = json_encode([
        'code' => 0,
        'msg' => "Hello {$name}",
    ] + $req_payload);
    $resp = [
        'HTTP/1.0 200 OK',
        'server: Swow/' . \Swow\VERSION,
        'content-length: ' . strlen($payload),
        'content-type: application/json',
        'connection: close',
        '',
        $payload,
    ];
    $conn->sendString(implode("\r\n", $resp));
    Assert::same($headers['x-ceshi-header'], 'cafebabe');
});

$ch = curl_init();
// curl it
Coroutine::run(function () use ($ch, $server) {
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
    $ret_str = curl_exec($ch);
    $ret = json_decode($ret_str, true);
    Assert::same($ret['code'], 0);
    Assert::same($ret['msg'], 'Hello Swow');
    Assert::same($ret['age'], 2);
    curl_close($ch);
});

echo 'Done' . PHP_LF;
?>
--EXPECT--
Done
