--TEST--
swow_thread: serialization and cloning are rejected
--SKIPIF--
<?php
if (!PHP_ZTS) {
    exit('skip ZTS only');
}
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

$thread = new Swow\Thread('var_dump', 'x');

try {
    serialize($thread);
} catch (Throwable $e) {
    echo $e::class, ': ', $e->getMessage(), "\n";
}

try {
    clone $thread;
} catch (Throwable $e) {
    echo $e::class, ': ', $e->getMessage(), "\n";
}
?>
--EXPECT--
Exception: Serialization of 'Swow\Thread' is not allowed
Error: Trying to clone an uncloneable object of class Swow\Thread
