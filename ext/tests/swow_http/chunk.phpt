--TEST--
swow_http: chunked encoding
--SKIPIF--
<?php

require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Buffer;
use Swow\Http\Parser;

$randoms = [''];
$seed = 42;
for ($i = 0; $i < 32; $i++) {
    // simple LCG with fixed seed
    $seed = (7 * $seed + 11) % 108;
    $chunk = random_bytes($seed);
    array_unshift($randoms, $chunk);
}
$body_full = "message_start1message_start2" . implode('', $randoms);

// generate a buffer holds our fake response
$buffer = new Buffer(4096);
$resp_lines = [
    'HTTP/1.1 200 OK',
    'Server: SomeChunkEncodingServer/0.1',
    'Content-Type: application/x-whatever',
    'Transfer-Encoding: chunked',
    '',
    'e',
    'message_start1',
    'E',
    'message_start2',
];
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

$body = "";

$i = 0;

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
            Assert::same($parser->getStatusCode(), 200);
            Assert::same($parser->getReasonPhrase(), "OK");
            Assert::same($status_str, "OK");
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
            Assert::same($parser->getProtocolVersion(), '1.1');
            Assert::same($parser->getMajorVersion(), 1);
            Assert::same($parser->getMinorVersion(), 1);
            Assert::false($parser->isUpgrade());
            //Assert::false($parser->shouldKeepAlive());
            break;
        case Parser::EVENT_BODY:
            $i++;
            $body .= $data;
            if ($i > 2) {
                Assert::same($data, $randoms[$i - 3]);
            }
            break;
    }
    if ($buffer->getReadableLength() === 0) {
        $now = $buffer->tell();
        $new = $randoms[$i - 2];
        $buffer->seek(0, SEEK_END);
        $buffer->write("\r\n" . sprintf("%x", strlen($new)) . "\r\n" . $new);
        $buffer->seek($now, SEEK_SET);
    }
}

Assert::same($body, $body_full);

Assert::true($parser->isCompleted());

// assert generated headers
$expected = [
    'Server' => ['SomeChunkEncodingServer/0.1'],
    'Content-Type' => ['application/x-whatever'],
    'Transfer-Encoding' => ['chunked'],
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
