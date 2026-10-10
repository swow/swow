--TEST--
swow_curl: multi option validation, callback ownership and handle reuse
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
skip_if(!Swow\Extension::isBuiltWith('curl'), 'extension must be built with libcurl');
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

Assert::throws(static function (): void {
    new CurlMultiHandle();
}, Error::class, expectMessage: '/curl_multi_init/');

$multi = curl_multi_init();
foreach ([
    CURLMOPT_MAXCONNECTS => 8,
    CURLMOPT_MAX_HOST_CONNECTIONS => 2,
    CURLMOPT_MAX_TOTAL_CONNECTIONS => 4,
    CURLMOPT_MAX_PIPELINE_LENGTH => 2,
    CURLMOPT_CHUNK_LENGTH_PENALTY_SIZE => 1024,
    CURLMOPT_CONTENT_LENGTH_PENALTY_SIZE => 1024,
    CURLMOPT_PIPELINING => CURLPIPE_MULTIPLEX,
] as $option => $value) {
    Assert::true(curl_multi_setopt($multi, $option, $value));
}
if (defined('CURLMOPT_MAX_CONCURRENT_STREAMS')) {
    Assert::true(curl_multi_setopt($multi, CURLMOPT_MAX_CONCURRENT_STREAMS, 2));
}
Assert::throws(static function () use ($multi): void {
    curl_multi_setopt($multi, PHP_INT_MAX, true);
}, ValueError::class, expectMessage: '/valid cURL multi option/');
Assert::notSame($invalidOptionError = curl_multi_errno($multi), CURLM_OK);
Assert::true(curl_multi_setopt($multi, CURLMOPT_MAXCONNECTS, 8));
Assert::same(curl_multi_errno($multi), CURLM_OK);
Assert::notSame(curl_multi_strerror(CURLM_OK), '');
Assert::notSame(curl_multi_strerror($invalidOptionError), '');
Assert::throws(static function () use ($multi): void {
    $copy = clone $multi;
}, Error::class, expectMessage: '/uncloneable/');

final class CurlPushOwner
{
    public function __construct(public CurlMultiHandle $multi)
    {
    }

    /** @param list<string> $headers */
    public function push(CurlHandle $parent, CurlHandle $pushed, array $headers): int
    {
        return CURL_PUSH_DENY;
    }
}

$first = new CurlPushOwner($multi);
$firstReference = WeakReference::create($first);
Assert::true(curl_multi_setopt($multi, CURLMOPT_PUSHFUNCTION, [$first, 'push']));
unset($first);
Assert::notNull($firstReference->get());
$second = new CurlPushOwner($multi);
$secondReference = WeakReference::create($second);
Assert::true(curl_multi_setopt($multi, CURLMOPT_PUSHFUNCTION, [$second, 'push']));
Assert::null($firstReference->get());
unset($second);
Assert::notNull($secondReference->get());
Assert::throws(static function () use ($multi): void {
    curl_multi_setopt($multi, CURLMOPT_PUSHFUNCTION, 'swow_missing_push_callback');
}, TypeError::class, expectMessage: '/valid callback/');
Assert::null($secondReference->get());

[$server, $coroutine] = createHttpTestServer();
try {
    $handle = curl_init('http://' . $server->getSockAddress() . ':' . $server->getSockPort() . '/multi-reuse');
    Assert::null(curl_multi_getcontent($handle));
    Assert::true(curl_setopt_array($handle, [
        CURLOPT_PROXY => '',
        CURLOPT_RETURNTRANSFER => true,
        CURLOPT_TIMEOUT_MS => 1000,
    ]));
    Assert::same(curl_multi_getcontent($handle), '');
    Assert::same(curl_multi_add_handle($multi, $handle), CURLM_OK);
    Assert::same(curl_multi_add_handle($multi, $handle), CURLM_ADDED_ALREADY);
    Assert::same(curl_multi_errno($multi), CURLM_ADDED_ALREADY);
    Assert::same(curl_multi_get_handles($multi), [$handle]);
    // Closing a multi handle detaches its transfers; the PHP objects remain reusable.
    curl_multi_close($multi);
    Assert::same(curl_multi_get_handles($multi), []);
    Assert::same(curl_multi_add_handle($multi, $handle), CURLM_OK);
    do {
        Assert::same(curl_multi_exec($multi, $running), CURLM_OK);
        if ($running > 0) {
            Assert::greaterThanEq(curl_multi_select($multi, 0.1), 0);
        }
    } while ($running > 0);
    $message = curl_multi_info_read($multi);
    Assert::same($message['handle'], $handle);
    Assert::same($message['result'], CURLE_OK);
    Assert::same(curl_multi_getcontent($handle), 'Done');
    $handleReference = WeakReference::create($handle);
    unset($message, $handle);
    Assert::notNull($handleReference->get());
    curl_multi_close($multi);
    Assert::null($handleReference->get());
} finally {
    unset($handle, $multi);
    $server->close();
    if ($coroutine->isAlive()) {
        $coroutine->kill();
    }
}

$multi = curl_multi_init();
$handle = curl_init();
$owner = new CurlPushOwner($multi);
$multiReference = WeakReference::create($multi);
$handleReference = WeakReference::create($handle);
$ownerReference = WeakReference::create($owner);
Assert::same(curl_multi_add_handle($multi, $handle), CURLM_OK);
Assert::true(curl_multi_setopt($multi, CURLMOPT_PUSHFUNCTION, [$owner, 'push']));
unset($multi, $handle, $owner);
gc_collect_cycles();
Assert::null($multiReference->get());
Assert::null($handleReference->get());
Assert::null($ownerReference->get());

echo "Done\n";
?>
--EXPECT--
Done
