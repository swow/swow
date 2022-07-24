--TEST--
swow_http: bad args passed in
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Http\ParserException;

// bad parser type in Parser::setType
Assert::throws(function () {
    $parser = new Swow\Http\Parser();
    $parser->setType(-1);
}, ValueError::class, expectMessage: '/Unknown/');

// bad method or protocol name in Parser::execute
Assert::throws(function () {
    $buffer = new Swow\Buffer(4096);
    $buffer->write("BREW /pot-2 HTCPCP/1.0\r\nAccept-Additions: Cream\r\n\r\n");
    $parser = new Swow\Http\Parser();
    $parser->execute($buffer->toString());
}, ParserException::class, expectMessage: '/Invalid method encountered/');

// bad status code in Parser::execute
Assert::throws(function () {
    $buffer = new Swow\Buffer(4096);
    $buffer->write("HTTP/1.1 12450 Blocked by 12dora\r\ncontent-type: coffee\r\n\r\n");
    $parser = new Swow\Http\Parser();
    $parser->execute($buffer->toString());
}, ParserException::class, expectMessage: '/Response overflow/');

// bad EOF in Parser::finish
Assert::throws(function () {
    $buffer = new Swow\Buffer(4096);
    $buffer->write("HTTP/1.0 201 Created\r\ncontent-type: application/json\r\ncontent-len");
    $parser = new Swow\Http\Parser();
    $parsedOffset = 0;
    while ($parsedOffset !== $buffer->getLength()) {
        $parsedOffset += $parser->execute($buffer->toString(), $parsedOffset);
    }
    $parser->finish();
}, ParserException::class, expectMessage: '/Invalid EOF state/');

echo 'Done' . PHP_LF;

?>
--EXPECT--
Done
