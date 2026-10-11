--TEST--
swow_fs: path queries preserve parameters, local paths and user stream wrappers
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
skip_if(!is_writable(sys_get_temp_dir()), 'temporary directory is not writable');
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

final class PathQueryStream
{
    public mixed $context;
    public static string $filename;
    public static int $calls = 0;

    public function url_stat(string $path, int $flags): array|false
    {
        Assert::same($path, 'pathquery://file');
        Assert::same($flags & STREAM_URL_STAT_QUIET, STREAM_URL_STAT_QUIET);
        self::$calls++;
        return @stat(self::$filename);
    }
}

$directory = sys_get_temp_dir() . DIRECTORY_SEPARATOR . 'swow-path-query-' . getRandomBytes(12);
$filename = $directory . DIRECTORY_SEPARATOR . '文件.txt';
$workingDirectory = getcwd();
try {
    Assert::true(mkdir($directory));
    Assert::same(file_put_contents($filename, 'contents'), 8);
    Assert::true(file_exists($filename));
    Assert::true(file_exists($directory));
    Assert::true(file_exists('file://' . $filename));
    Assert::true(is_string(realpath($filename)));
    Assert::true(chdir($directory));
    Assert::true(file_exists('./文件.txt'));
    Assert::same(realpath('./文件.txt'), realpath($filename));
    Assert::same(realpath(''), realpath('.'));

    $completion = new Swow\Channel(1);
    $coroutine = Swow\Coroutine::run(static function () use ($filename, $completion): void {
        $completion->push([file_exists($filename), is_string(realpath($filename))]);
    });
    $yielded = $coroutine->isAlive();
    Assert::same($completion->pop(1000), [true, true]);
    // Native existence and path queries must not introduce an implicit coroutine suspension.
    Assert::false($yielded);

    $path = new class($filename) {
        public int $calls = 0;

        public function __construct(public string $filename)
        {
        }

        public function __toString(): string
        {
            $this->calls++;
            return $this->filename;
        }
    };
    Assert::true(file_exists($path));
    Assert::same($path->calls, 1);
    Assert::same(realpath($path), realpath($filename));
    Assert::same($path->calls, 2);
    Assert::false(file_exists(''));
    Assert::false(file_exists("invalid\0path"));
    Assert::throws(static fn(): string|false => realpath("invalid\0path"), ValueError::class);
    foreach (['file_exists', 'realpath'] as $query) {
        Assert::throws(static fn(): mixed => $query(), ArgumentCountError::class);
        Assert::throws(static fn(): mixed => $query([]), TypeError::class);
    }

    PathQueryStream::$filename = $filename;
    Assert::true(stream_wrapper_register('pathquery', PathQueryStream::class));
    Assert::true(file_exists('pathquery://file'));
    Assert::same(PathQueryStream::$calls, 1);
    Assert::true(unlink($filename));
    clearstatcache(true);
    Assert::false(file_exists($filename));
    Assert::false(realpath($filename));
    Assert::false(file_exists('pathquery://file'));
    Assert::same(PathQueryStream::$calls, 2);
} finally {
    if (in_array('pathquery', stream_get_wrappers(), true)) {
        stream_wrapper_unregister('pathquery');
    }
    chdir($workingDirectory);
    if (file_exists($filename)) {
        unlink($filename);
    }
    if (is_dir($directory)) {
        rmdir($directory);
    }
}
echo "Done\n";
?>
--EXPECT--
Done
