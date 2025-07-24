--TEST--
swow_fs: fileno()
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--INI--
phar.readonly=0
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';
require_once __DIR__ . '/build_phar.inc';

use function Swow\fileno;
use function Swow\pipe;

// may be any number in PHPT
var_dump(fileno(STDIN));
var_dump(fileno(STDOUT));
var_dump(fileno(STDERR));

$stdin = fopen('php://stdin', 'r');
var_dump(fileno($stdin));
$stdout = fopen('php://stdout', 'w');
var_dump(fileno($stdout));
$stderr = fopen('php://stderr', 'w');
var_dump(fileno($stderr));

$context = stream_context_create();
Assert::throws(static function () use ($context): void {
    fileno($context);
}, Swow\Exception::class, expectMessage: 'Invalid stream resource');

$memory = fopen('php://memory', 'r+');
Assert::throws(static function () use ($memory): void {
    fileno($memory);
}, Swow\Exception::class, expectMessage: 'Cannot represent a stream of type MEMORY as a File Descriptor');

// maybe we can support this in the future ?
$url = fopen(TEST_WEBSITE1_URL, 'r');
Assert::throws(static function () use ($url): void {
    fileno($url);
}, Swow\Exception::class, expectMessage: 'Cannot represent a stream of type tcp_socket/ssl as a File Descriptor');

if (extension_loaded('phar')) {
    build_phar(__DIR__ . DIRECTORY_SEPARATOR . 'fileno_test.phar', TEST_REQUIRE);
    $phar = fopen('phar://' . __DIR__ . DIRECTORY_SEPARATOR . 'fileno_test.phar' . DIRECTORY_SEPARATOR . 'run.php', 'r');
    Assert::throws(static function () use ($phar): void {
        fileno($phar);
    }, Swow\Exception::class, expectMessage: 'Cannot represent a stream of type phar stream as a File Descriptor');
}

class TestUserStream
{
    /* resource */ public $context;

    public function stream_open(
        string $path,
        string $mode,
        int $options,
        ?string &$opened_path,
    ): bool {
        return true;
    }

    public function stream_close(): void
    {
    }

    public function stream_cast(int $cast_as): mixed
    {
        throw new TestError('cafebabe');
    }
}
stream_wrapper_register('test', TestUserStream::class);
$test = fopen('test://test', 'r');
Assert::throws(static function () use ($test): void {
    fileno($test);
}, Swow\Exception::class, expectMessage: 'Cannot represent a stream of type user-space as a File Descriptor');
stream_wrapper_unregister('test');

[$r, $w] = pipe();
var_dump(fileno($r));
var_dump(fileno($w));

// I have no idea how to test "Stream is not associated with a file descriptor"

echo 'Done' . PHP_EOL;
?>
--CLEAN--
<?php
@unlink(__DIR__ . '/fileno_test.txt');
@unlink(__DIR__ . '/fileno_test.phar');
?>
--EXPECTF--
int(%d)
int(%d)
int(%d)
int(%d)
int(%d)
int(%d)
int(%d)
int(%d)
Done
