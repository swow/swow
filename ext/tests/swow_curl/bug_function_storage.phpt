--TEST--
swow_curl: callback function storage
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
skip_if(PHP_SAPI !== 'cli', 'only for cli');
skip_if(!Swow\Extension::isBuiltWith('curl'), 'extension must be built with libcurl');
require __DIR__ . '/../include/bootstrap.php';
skip_if(!str_contains(@file_get_contents(TEST_WEBSITE1_URL), TEST_WEBSITE1_KEYWORD), 'Unable to access ' . TEST_WEBSITE1_URL);
?>
--FILE--
<?php
require_once __DIR__ . '/../include/bootstrap.php';

$testHeaderFunction = static function (CurlHandle $curl, string $headerLine): int {
    return testHeaderFunction($curl, $headerLine);
};

function testHeaderFunction(CurlHandle $curl, string $headerLine): int
{
    $parts = explode(':', $headerLine, 2);
    if (count($parts) === 2) {
        $GLOBALS['header_lines'][strtolower($parts[0])] = trim($parts[1]);
    }
    return strlen($headerLine);
}

class testHeaderFunctionClass
{
    public static function headerFunction(CurlHandle $curl, string $headerLine): int
    {
        return testHeaderFunction($curl, $headerLine);
    }
}

foreach ([$testHeaderFunction, 'testHeaderFunction', [testHeaderFunctionClass::class, 'headerFunction']] as $headerFunction) {
    $curl = curl_init();
    curl_setopt($curl, CURLOPT_URL, TEST_WEBSITE1_URL);
    curl_setopt($curl, CURLOPT_HEADERFUNCTION, $headerFunction);
    curl_setopt($curl, CURLOPT_RETURNTRANSFER, 1);
    unset($testHeaderFunction);
    $response = curl_exec($curl);
    Assert::contains($response, TEST_WEBSITE1_KEYWORD);
    Assert::true(strtotime($GLOBALS['header_lines']['date']) > 0);
    if (PHP_VERSION_ID < 80100) {
        curl_close($curl);
    }
}

$curl = curl_init();
curl_setopt($curl, CURLOPT_URL, TEST_WEBSITE1_URL);
curl_setopt($curl, CURLOPT_HEADERFUNCTION, 'testHeaderFunction');
curl_setopt($curl, CURLOPT_HEADERFUNCTION, null);

echo "Done\n";
?>
--EXPECT--
Done
