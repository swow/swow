--TEST--
swow_http: response parser functionality
--SKIPIF--
<?php

require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Buffer;
use Swow\Http\Parser;

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

// create parser
$parser = new Parser();
// (optional) tell parser parse a response
$parser->setType(Parser::TYPE_RESPONSE);
Assert::same($parser->getType(), Parser::TYPE_RESPONSE);
// subscribe to all events
$parser->setEvents(Parser::EVENTS_ALL);
Assert::same($parser->getEvents(), Parser::EVENTS_ALL);

$headers = [];

// parse it
// Parser::execute reads from buffer, then generate an event if subscribed
while (Parser::EVENT_MESSAGE_COMPLETE !== ($event = $parser->execute($buffer))) {
    Assert::same($event, $parser->getEvent());
    Assert::string($parser->getEventName());
    //var_dump($parser->getEventName());
    // read data from buffer according to parser
    $data = '';
    if (Parser::EVENT_FLAG_DATA & $event) {
        $data = $buffer->peekFrom($parser->getDataOffset(), $parser->getDataLength());
    }
    switch ($event) {
        case Parser::EVENT_STATUS:
            // parse status
            $status_str = $data;
            Assert::same($parser->getStatusCode(), 418);
            Assert::same($parser->getReasonPhrase(), "I'm a teapot");
            Assert::same($status_str, "I'm a teapot");
            break;
        case Parser::EVENT_HEADER_FIELD:
            // start parse headers
            $k = $data;
            break;
        case Parser::EVENT_HEADER_VALUE:
            // fill key
            $headers[$k][] = $data;
            break;
        case Parser::EVENT_HEADERS_COMPLETE:
            // content length
            // note: it may be cleaned after this event, so it's recommended to use headers['content-length']
            Assert::same($parser->getContentLength(), strlen($payload));
            Assert::same($parser->getProtocolVersion(), '1.1');
            Assert::same($parser->getMajorVersion(), 1);
            Assert::same($parser->getMinorVersion(), 1);
            Assert::false($parser->isUpgrade());
            Assert::false($parser->shouldKeepAlive());
            break;
        case Parser::EVENT_BODY:
            $body = $data;
            Assert::same($body, $payload);
            break;
    }
}

Assert::true($parser->isCompleted());

// assert generated headers
$expected = [
    'Server' => ['alpine-linux.local'],
    'Connection' => ['close'],
    'X-Test-Header' => ['value1', 'value2'],
    'Content-Type' => ['application/json'],
    'Content-Length' => [(string) strlen($payload)],
];

foreach($expected as $k => $expected_values){
    $real_values = $headers[$k];
    sort($real_values);
    sort($expected_values);
    Assert::same($real_values, $expected_values);
}

echo 'Done' . PHP_LF;
?>
--EXPECT--
Done
