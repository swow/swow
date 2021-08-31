--TEST--
swow_http: parser automatic type
--SKIPIF--
<?php

require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Buffer;
use Swow\Http\Parser;

// create parser
$parser = new Parser();

// generate a buffer holds our fake request
$buffer = new Buffer(4096);
$payload = json_encode([
    'container' => 'mug',
    'capacity' => 500,
]);
$req_lines = [
    'GET /pot-0?additions[]=Cream HTTP/1.1',
    'Server: alpine-linux.local',
    'Connection: close',
    'Accept-Additions: Cream',
    'X-Test-Header: value1',
    'X-Test-Header: value2',
    'Content-Type: application/json',
    'Content-Length: ' . strlen($payload),
    '',
    $payload,
];
//var_dump(implode("\r\n", $req_lines));
$buffer->write(implode("\r\n", $req_lines));
$buffer->rewind();

$parser->execute($buffer);
Assert::same($parser->getType(), Parser::TYPE_REQUEST);

$parser->reset();
$parser->setType(Parser::TYPE_BOTH);

// generate a buffer holds our fake response
$buffer = new Buffer(4096);
$payload = json_encode([
    'code' => 418,
    'msg' => "I'm a teapot",
]);
$resp_lines = [
    'HTTP/1.1 418 I\'m a teapot',
    'Server: alpine-linux.local',
    'Connection: close',
    'X-Test-Header: value1',
    'X-Test-Header: value2',
    'Content-Type: application/json',
    'Content-Length: ' . strlen($payload),
    '',
    $payload,
];
//var_dump(implode("\r\n", $resp_lines));
$buffer->write(implode("\r\n", $resp_lines));
$buffer->rewind();

$parser->execute($buffer);
Assert::same($parser->getType(), Parser::TYPE_RESPONSE);

$parser->finish();

echo 'Done' . PHP_LF;
?>
--EXPECT--
Done
