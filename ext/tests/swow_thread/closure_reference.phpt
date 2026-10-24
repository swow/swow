--TEST--
swow_thread: a closure body with a by-reference capture runs in the thread
--SKIPIF--
<?php
if (getenv('SWOW_TSAN') !== false) {
    exit('skip TSan cannot handle PHP bailout');
}
if (!PHP_ZTS) {
    exit('skip ZTS only');
}
require __DIR__ . '/../include/skipif.php';
?>
--INI--
swow.closure_serializer=1
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Thread;

$counter = 0;
$thread = new Thread(static function () use (&$counter): string {
    $counter++;

    return 'captured';
});
$thread->start();

// the unserialize of a by-reference use needs a symbol table that a call from C
// does not have
Assert::same('captured', $thread->wait());
Assert::same(0, $thread->getExitStatus());

echo "done\n";
?>
--EXPECT--
done
