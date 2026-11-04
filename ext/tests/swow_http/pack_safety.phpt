--TEST--
swow_http: pack helpers reject header values with CRLF and keep a stable size
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Http\Http;

// A header value whose __toString() returns a different length on each call
// must not overflow the packed message
class GrowingStringable
{
    public int $calls = 0;

    public function __toString(): string
    {
        return str_repeat('a', ++$this->calls === 1 ? 4 : 4096);
    }
}

$stringable = new GrowingStringable();
$packed = Http::packRequest(
    method: 'GET',
    uri: '/',
    headers: ['X-Test' => [$stringable]]
);
// the value is whatever the first conversion produced, and the message is
// built without a length pass, so a growing value cannot overflow it
Assert::same($packed, "GET / HTTP/1.1\r\nX-Test: aaaa\r\n\r\n");
Assert::same(
    Http::packResponse(statusCode: 200, headers: ['X-Test' => [new GrowingStringable()]]),
    "HTTP/1.1 200 OK\r\nX-Test: aaaa\r\n\r\n"
);

// CRLF injection through a header name or value must be rejected.
Assert::throws(static function (): void {
    Http::packRequest(method: 'GET', uri: '/', headers: ["X-Bad\r\nInjected: 1" => 'v']);
}, ValueError::class);
Assert::throws(static function (): void {
    Http::packRequest(method: 'GET', uri: '/', headers: ['X-Bad' => "v\r\nInjected: 1"]);
}, ValueError::class);
Assert::throws(static function (): void {
    Http::packRequest(method: "GET\r\nInjected", uri: '/');
}, ValueError::class);
Assert::throws(static function (): void {
    Http::packRequest(method: 'GET', uri: "/\r\nInjected");
}, ValueError::class);
Assert::throws(static function (): void {
    Http::packResponse(statusCode: 200, reasonPhrase: "OK\r\nInjected: 1");
}, ValueError::class);
Assert::throws(static function (): void {
    Http::packResponse(statusCode: 200, headers: ['X-Bad' => "v\r\nInjected: 1"]);
}, ValueError::class);

// The usual path is unchanged.
Assert::same(
    Http::packRequest(method: 'GET', uri: '/', headers: ['Host' => 'example.com'], body: 'hi'),
    "GET / HTTP/1.1\r\nHost: example.com\r\n\r\nhi"
);
Assert::same(
    Http::packResponse(statusCode: 200, headers: ['Host' => 'example.com'], body: 'hi'),
    "HTTP/1.1 200 OK\r\nHost: example.com\r\n\r\nhi"
);

echo "Done\n";

?>
--EXPECT--
Done
