--TEST--
swow_fs: file stream offsets, truncation, copying and failure recovery
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
skip_if(!is_writable(sys_get_temp_dir()), 'temporary directory is not writable');
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

$directory = sys_get_temp_dir() . '/swow-stream-' . getRandomBytes(12);
$source = $directory . '/source';
$destination = $directory . '/destination';
$missing = $directory . '/missing';
$input = $output = null;
try {
    Assert::true(mkdir($directory));
    $body = str_repeat("a\0b\r\nc", 2048);
    Assert::same(file_put_contents($source, $body), strlen($body));
    Assert::same(file_get_contents($source, offset: 2, length: 9), substr($body, 2, 9));
    $input = fopen($source, 'r+b');
    $output = fopen($destination, 'w+b');
    Assert::true(stream_get_meta_data($input)['seekable']);
    Assert::same(fstat($input)['size'], strlen($body));
    Assert::same(stream_get_contents($input, 4, 2), substr($body, 2, 4));
    Assert::same(ftell($input), 6);
    Assert::same(fseek($input, -2, SEEK_CUR), 0);
    Assert::same(ftell($input), 4);
    Assert::same(stream_copy_to_stream($input, $output, 7), 7);
    Assert::true(rewind($output));
    Assert::same(stream_get_contents($output), substr($body, 4, 7));
    Assert::same(fseek($input, 0, SEEK_END), 0);
    Assert::same(ftell($input), strlen($body));
    Assert::same(fread($input, 1), '');
    Assert::true(feof($input));
    Assert::true(rewind($input));
    Assert::false(feof($input));
    Assert::true(ftruncate($input, 3));
    Assert::same(fstat($input)['size'], 3);
    Assert::true(ftruncate($input, 6));
    Assert::true(rewind($input));
    Assert::same(stream_get_contents($input), "a\0b\0\0\0");
    Assert::true(fclose($input));
    $input = fopen($source, 'a+b');
    Assert::true(rewind($input));
    Assert::same(fwrite($input, 'tail'), 4);
    Assert::true(fflush($input));
    Assert::same(file_get_contents($source), "a\0b\0\0\0tail");
    Assert::true(rewind($input));
    Assert::same(fstat($input)['size'], 10);

    $warnings = [];
    set_error_handler(static function (int $severity, string $message) use (&$warnings): bool {
        Assert::same($severity, E_WARNING);
        $warnings[] = $message;
        return true;
    });
    try {
        Assert::false(unlink($missing));
        Assert::false(rename($missing, $destination));
        Assert::false(rmdir($missing));
        Assert::false(stat($missing));
    } finally {
        restore_error_handler();
    }
    Assert::same(count($warnings), 4);
    foreach ($warnings as $warning) {
        Assert::contains($warning, 'missing');
    }
    Assert::true(rewind($input));
    Assert::same(stream_get_contents($input), "a\0b\0\0\0tail");
    Assert::same(file_get_contents($destination), substr($body, 4, 7));
} finally {
    foreach ([$input, $output] as $stream) {
        if (is_resource($stream)) {
            fclose($stream);
        }
    }
    foreach ([$source, $destination] as $filename) {
        if (file_exists($filename)) {
            unlink($filename);
        }
    }
    if (is_dir($directory)) {
        rmdir($directory);
    }
}
echo "Done\n";
?>
--EXPECT--
Done
