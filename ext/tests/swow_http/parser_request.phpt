--TEST--
swow_http: request parser functionality
--SKIPIF--
<?php

require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Buffer;
use Swow\Http\Parser;

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
$buffer->append(implode("\r\n", $req_lines));

// create parser
$parser = new Parser();
// (optional) tell parser parse a request
$parser->setType(Parser::TYPE_REQUEST);
Assert::same($parser->getType(), Parser::TYPE_REQUEST);
// subscribe to all events
$parser->setEvents(Parser::EVENTS_ALL);
Assert::same($parser->getEvents(), Parser::EVENTS_ALL);

$headers = [];

// parse it
// Parser::execute reads from buffer, then generate an event if subscribed
$parsedOffset = 0;
while (true) {
    $parsedOffset += $parser->execute($buffer->toString(), $parsedOffset);
    if (Parser::EVENT_MESSAGE_COMPLETE === ($event = $parser->getEvent())) {
        break;
    }
    Assert::integer($event);
    Assert::string($parser->getEventName());
    Assert::same($parser::getEventNameFor($event), $parser->getEventName());
    // read data from buffer according to parser
    $data = '';
    if (Parser::EVENT_FLAG_DATA & $event) {
        $data = $buffer->read($parser->getDataOffset(), $parser->getDataLength());
    }
    switch ($event) {
        case Parser::EVENT_URL:
            // parse url
            $url = $data;
            Assert::same($parser->getMethod(), 'GET');
            Assert::same($url, '/pot-0?additions[]=Cream');
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
    'Accept-Additions' => ['Cream'],
    'X-Test-Header' => ['value1', 'value2'],
    'Content-Type' => ['application/json'],
    'Content-Length' => [(string) strlen($payload)],
];

foreach ($expected as $k => $expected_values) {
    $real_values = $headers[$k];
    sort($real_values);
    sort($expected_values);
    Assert::same($real_values, $expected_values);
}

echo "Done\n";
?>
--EXPECT--
Done
