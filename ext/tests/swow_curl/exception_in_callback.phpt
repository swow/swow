--TEST--
swow_curl: exception in callback
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
skip_if(!Swow\Extension::isBuiltWith('curl'), 'extension must be built with libcurl');
require __DIR__ . '/../include/bootstrap.php';
skip_if(!str_contains(@file_get_contents(TEST_WEBSITE2_URL), TEST_WEBSITE2_KEYWORD), 'Unable to access ' . TEST_WEBSITE2_URL);
needs_php_version('>=', '8.1');
?>
--FILE--
<?php
require_once __DIR__ . '/../include/bootstrap.php';

$ch = curl_init();

curl_setopt($ch, CURLOPT_URL, TEST_WEBSITE2_URL);
curl_setopt($ch, CURLOPT_RETURNTRANSFER, 1);
curl_setopt($ch, CURLOPT_HEADERFUNCTION, static function (CurlHandle $curl, string $headerLine): int {
    debug_print_backtrace();
    throw new Exception('testh');
});

Assert::throws(static function () use ($ch): void {
    curl_exec($ch);
}, Exception::class, expectMessage: 'testh');

$ch = curl_init();

curl_setopt($ch, CURLOPT_URL, TEST_WEBSITE2_URL);
curl_setopt($ch, CURLOPT_RETURNTRANSFER, 1);
curl_setopt($ch, CURLOPT_WRITEFUNCTION, static function (CurlHandle $curl, string $data): int {
    debug_print_backtrace();
    throw new Exception('testw');
});

Assert::throws(static function () use ($ch): void {
    curl_exec($ch);
}, Exception::class, expectMessage: 'testw');

$ch = curl_init();

curl_setopt($ch, CURLOPT_URL, TEST_WEBSITE2_URL);
curl_setopt($ch, CURLOPT_RETURNTRANSFER, 1);
curl_setopt($ch, CURLOPT_UPLOAD, 1);
curl_setopt($ch, CURLOPT_INFILE, STDIN);
curl_setopt($ch, CURLOPT_READFUNCTION, static function (CurlHandle $curl, mixed $fd, int $length): int {
    debug_print_backtrace();
    throw new Exception('testr');
});

Assert::throws(static function () use ($ch): void {
    curl_exec($ch);
}, Exception::class, expectMessage: 'testr');

echo "Done\n";
?>
--EXPECTF--
#0 [internal function]: {closur%s}(Object(CurlHandle), '%A')
#1 %sexception_in_callback.php(%d): curl_exec(Object(CurlHandle))
#2 %s(%d): {closur%s}()
#3 %sexception_in_callback.php(%d): Assert::throws(Object(Closure), 'Exception', NULL, 'testh')
#0 [internal function]: {closur%s}(Object(CurlHandle), '%A')
#1 %sexception_in_callback.php(%d): curl_exec(Object(CurlHandle))
#2 %s(%d): {closur%s}()
#3 %sexception_in_callback.php(%d): Assert::throws(Object(Closure), 'Exception', NULL, 'testw')
#0 [internal function]: {closur%s}(Object(CurlHandle), Resource id #%d, %d)
#1 %sexception_in_callback.php(%d): curl_exec(Object(CurlHandle))
#2 %s(%d): {closur%s}()
#3 %sexception_in_callback.php(%d): Assert::throws(Object(Closure), 'Exception', NULL, 'testr')
Done
