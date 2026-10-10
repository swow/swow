--TEST--
swow_curl: persistent share options, validation, reuse and transfer ownership
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
skip_if(!Swow\Extension::isBuiltWith('curl'), 'extension must be built with libcurl');
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

foreach ([
    [[], ValueError::class, '/must not be empty/'],
    [[CURL_LOCK_DATA_COOKIE], ValueError::class, '/sharing cookies across PHP requests is unsafe/'],
    [[-1], ValueError::class, '/CURL_LOCK_DATA_\*/'],
    [['invalid'], TypeError::class, '/must contain only int values/'],
] as [$options, $class, $message]) {
    Assert::throws(static function () use ($options): void {
        curl_share_init_persistent($options);
    }, $class, expectMessage: $message);
}
Assert::throws(static function (): void {
    new CurlSharePersistentHandle();
}, Error::class, expectMessage: '/curl_share_init_persistent/');

$options = [CURL_LOCK_DATA_DNS, CURL_LOCK_DATA_SSL_SESSION, CURL_LOCK_DATA_CONNECT];
if (curl_version()['features'] & CURL_VERSION_PSL) {
    $options[] = CURL_LOCK_DATA_PSL;
}
$first = curl_share_init_persistent(array_merge(array_reverse($options), [CURL_LOCK_DATA_DNS]));
$second = curl_share_init_persistent($options);
Assert::isInstanceOf($first, CurlSharePersistentHandle::class);
Assert::notSame($first, $second);
Assert::same($first->options, $options);
Assert::same($second->options, $options);
Assert::throws(static function () use ($first): void {
    $first->options = [];
}, Error::class, expectMessage: '/readonly property/');
Assert::throws(static function () use ($first): void {
    $copy = clone $first;
}, Error::class, expectMessage: '/uncloneable/');

[$server, $coroutine] = createHttpTestServer();
$url = 'http://' . $server->getSockAddress() . ':' . $server->getSockPort() . '/persistent';
$handles = [curl_init($url), curl_init($url)];
try {
    foreach ($handles as $index => $handle) {
        Assert::true(curl_setopt_array($handle, [
            CURLOPT_SHARE => $index === 0 ? $first : $second,
            CURLOPT_PROXY => '',
            CURLOPT_RETURNTRANSFER => true,
            CURLOPT_TIMEOUT_MS => 1000,
        ]));
    }
    // Transfers retain their PHP share wrappers, while the persistent pool owns the native cache.
    $weak = WeakReference::create($first);
    unset($first, $second);
    Assert::notNull($weak->get());
    foreach ($handles as $handle) {
        Assert::same(curl_exec($handle), 'Done');
        Assert::same(curl_errno($handle), CURLE_OK);
    }
    unset($handle, $handles);
    Assert::null($weak->get());
    $reused = curl_share_init_persistent($options);
    Assert::same($reused->options, $options);
    unset($reused);
} finally {
    unset($handle, $handles, $first, $second);
    $server->close();
    if ($coroutine->isAlive()) {
        $coroutine->kill();
    }
}
echo "Done\n";
?>
--EXPECT--
Done
