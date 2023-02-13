--TEST--
swow_stream: wrapper_register
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--XFAIL--
Need to fix
--FILE--
<?php
class UserStream
{
    public mixed $context = null;

    public function stream_open($path, $mode, $options, &$opened_path)
    {
        return true;
    }

    public function stream_read($count)
    {
        return 0;
    }

    public function stream_eof()
    {
        return true;
    }

    public function stream_flush(): void
    {
        Assert::true(false);
    }

    public function stream_close(): void
    {
        Swow\Coroutine::getCurrent();
    }
}

stream_wrapper_register('UserStream', UserStream::class);
$fp = fopen('UserStream://foo', 'r');

?>
--EXPECT--
Done
