--TEST--
swow_http: getReasonPhrase
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

Assert::same(Swow\Http\Status::getReasonPhraseFor(-1), 'UNKNOWN');
Assert::same(Swow\Http\Status::getReasonPhraseFor(200), 'OK');
Assert::same(Swow\Http\Status::getReasonPhraseFor(404), 'Not Found');
Assert::same(Swow\Http\Status::getReasonPhraseFor(502), 'Bad Gateway');
Assert::same(Swow\Http\Status::getReasonPhraseFor(418), "I'm a teapot");

echo 'Done' . PHP_LF;
?>
--EXPECT--
Done

