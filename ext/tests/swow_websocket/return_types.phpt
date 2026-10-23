--TEST--
swow_websocket: opcode and status return types
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\WebSocket\Opcode;
use Swow\WebSocket\Status;

$opcode = new ReflectionMethod(Opcode::class, 'getNameOf');
var_dump((string) $opcode->getReturnType());
var_dump(get_debug_type(Opcode::getNameOf(1)));

$status = new ReflectionMethod(Status::class, 'getDescriptionOf');
var_dump((string) $status->getReturnType());
var_dump(get_debug_type(Status::getDescriptionOf(1000)));
?>
--EXPECT--
string(6) "string"
string(6) "string"
string(6) "string"
string(6) "string"
