--TEST--
swow_curl: easy (SSL connection)
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
skip_if(PHP_SAPI !== 'cli', 'only for cli');
skip_if(!Swow\Extension::isBuiltWith('curl'), 'extension must be built with libcurl');
require __DIR__ . '/../include/bootstrap.php';
skip_if(!str_contains(@file_get_contents(TEST_WEBSITE1_URL), TEST_WEBSITE1_KEYWORD), 'Unable to access ' . TEST_WEBSITE1_URL);
skip_if(!str_contains(@file_get_contents(TEST_WEBSITE2_URL), TEST_WEBSITE2_KEYWORD), 'Unable to access ' . TEST_WEBSITE2_URL);
?>
--FILE--
<?php
require_once __DIR__ . '/../include/bootstrap.php';

$ch1 = curl_init();
$ch2 = curl_init();

curl_setopt($ch1, CURLOPT_URL, TEST_WEBSITE1_URL);
curl_setopt($ch1, CURLOPT_HEADER, 0);
curl_setopt($ch1, CURLOPT_RETURNTRANSFER, 1);
curl_setopt($ch2, CURLOPT_URL, TEST_WEBSITE2_URL);
curl_setopt($ch2, CURLOPT_HEADER, 0);
curl_setopt($ch2, CURLOPT_RETURNTRANSFER, 1);

$response1 = curl_exec($ch1);
$response2 = curl_exec($ch2);

Assert::contains($response1, TEST_WEBSITE1_KEYWORD);
Assert::contains($response2, TEST_WEBSITE2_KEYWORD);

echo "Done\n";
?>
--EXPECT--
Done
