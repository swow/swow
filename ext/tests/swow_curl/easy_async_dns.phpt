--TEST--
swow_curl: easy with async DNS
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
skip_if(PHP_SAPI !== 'cli', 'only for cli');
skip_if(!Swow\Extension::isBuiltWith('curl'), 'extension must be built with libcurl');
require __DIR__ . '/../include/bootstrap.php';
skip_if(!(curl_version()['features'] & CURL_VERSION_ASYNCHDNS), 'libcurl must be built with async DNS');
skip_if(!str_contains(@file_get_contents(TEST_WEBSITE1_URL), TEST_WEBSITE1_KEYWORD), 'Unable to access ' . TEST_WEBSITE1_URL);
?>
--FILE--
<?php
require_once __DIR__ . '/../include/bootstrap.php';

$ch = curl_init();
Assert::notSame($ch, false);
Assert::same(curl_setopt_array($ch, [
    CURLOPT_URL => TEST_WEBSITE1_URL,
    CURLOPT_RETURNTRANSFER => true,
    CURLOPT_CONNECTTIMEOUT => 3,
    CURLOPT_TIMEOUT => 5,
    CURLOPT_SSL_VERIFYPEER => false,
    CURLOPT_SSL_VERIFYHOST => 0,
]), true);

$response = curl_exec($ch);

Assert::same(curl_errno($ch), 0, curl_error($ch));
Assert::contains($response, TEST_WEBSITE1_KEYWORD);

echo "Done\n";
?>
--EXPECT--
Done
