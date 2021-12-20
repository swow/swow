--TEST--
swow_http: packMessage packRequest packResponse
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Http;

$payload = 'hello world';
$headers = [
    'Server' => 'alpine-linux.local',
    'Connection' => 'close',
    'Accept-Additions' => 'Cream',
    'X-Test-Header' => ['value1', 'value2'],
    'Content-Type' => 'application/json',
    'Content-Length' => strlen($payload),
    '',
    $payload,
];

var_dump(Http\packMessage($headers, $payload));
var_dump(Http\packRequest('GET', '/some#path?a=b&c=d', $headers, $payload));
var_dump(Http\packResponse(418, $headers, $payload, "I'm apt, not apk", '1.0'));

echo 'Done' . PHP_LF;
?>
--EXPECT--
string(183) "Server: alpine-linux.local
Connection: close
Accept-Additions: Cream
X-Test-Header: value1
X-Test-Header: value2
Content-Type: application/json
Content-Length: 11

hello world"
string(216) "GET /some#path?a=b&c=d HTTP/1.1
Server: alpine-linux.local
Connection: close
Accept-Additions: Cream
X-Test-Header: value1
X-Test-Header: value2
Content-Type: application/json
Content-Length: 11

hello world"
string(214) "HTTP/1.0 418 I'm apt, not apk
Server: alpine-linux.local
Connection: close
Accept-Additions: Cream
X-Test-Header: value1
X-Test-Header: value2
Content-Type: application/json
Content-Length: 11

hello world"
Done
