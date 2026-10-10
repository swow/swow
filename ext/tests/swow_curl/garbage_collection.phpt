--TEST--
swow_curl: callback cycles and copied handle ownership are collected
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
skip_if(!Swow\Extension::isBuiltWith('curl'), 'extension must be built with libcurl');
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

final class CurlCallbackOwner
{
    public function __construct(public CurlHandle $handle)
    {
    }

    public function read(CurlHandle $handle, mixed $stream, int $length): string
    {
        return '';
    }

    public function write(CurlHandle $handle, string $data): int
    {
        return strlen($data);
    }

    public function progress(CurlHandle $handle, float $downloadTotal, float $downloaded, float $uploadTotal, float $uploaded): int
    {
        return 0;
    }

    public function xferinfo(CurlHandle $handle, int $downloadTotal, int $downloaded, int $uploadTotal, int $uploaded): int
    {
        return 0;
    }

    public function fnmatch(CurlHandle $handle, string $pattern, string $filename): int
    {
        return CURL_FNMATCHFUNC_MATCH;
    }

    public function debug(CurlHandle $handle, int $type, string $data): void
    {
    }

    public function prereq(CurlHandle $handle, string $primaryIp, string $localIp, int $primaryPort, int $localPort): int
    {
        return CURL_PREREQFUNC_OK;
    }
}

$handle = curl_init();
$owner = new CurlCallbackOwner($handle);
$handleReference = WeakReference::create($handle);
$ownerReference = WeakReference::create($owner);
Assert::true(curl_setopt_array($handle, [
    CURLOPT_READFUNCTION => [$owner, 'read'],
    CURLOPT_WRITEFUNCTION => [$owner, 'write'],
    CURLOPT_HEADERFUNCTION => [$owner, 'write'],
    CURLOPT_PROGRESSFUNCTION => [$owner, 'progress'],
    CURLOPT_XFERINFOFUNCTION => [$owner, 'xferinfo'],
    CURLOPT_FNMATCH_FUNCTION => [$owner, 'fnmatch'],
    CURLOPT_PRIVATE => $owner,
]));
if (defined('CURLOPT_DEBUGFUNCTION')) {
    Assert::true(curl_setopt($handle, CURLOPT_DEBUGFUNCTION, [$owner, 'debug']));
}
if (defined('CURLOPT_PREREQFUNCTION')) {
    Assert::true(curl_setopt($handle, CURLOPT_PREREQFUNCTION, [$owner, 'prereq']));
}
$copy = curl_copy_handle($handle);
Assert::same(curl_getinfo($copy, CURLINFO_PRIVATE), $owner);
unset($handle, $owner);
gc_collect_cycles();
// The copy retains callbacks that refer to the original handle through their owner.
Assert::notNull($handleReference->get());
Assert::notNull($ownerReference->get());
unset($copy);
gc_collect_cycles();
Assert::null($handleReference->get());
Assert::null($ownerReference->get());

$handle = curl_init();
$owner = new CurlCallbackOwner($handle);
$handleReference = WeakReference::create($handle);
$ownerReference = WeakReference::create($owner);
Assert::true(curl_setopt($handle, CURLOPT_WRITEFUNCTION, [$owner, 'write']));
unset($handle, $owner);
gc_collect_cycles();
Assert::null($handleReference->get());
Assert::null($ownerReference->get());
echo "Done\n";
?>
--EXPECT--
Done
