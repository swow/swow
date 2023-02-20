--TEST--
swow_curl: easy (SSL connection)
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
skip_if_extension_not_exist('curl');
skip_if(PHP_SAPI !== 'cli', 'only for cli');
skip_if(!getenv('SWOW_HAVE_CURL') && !Swow\Extension::isBuiltWith('curl'), 'extension must be built with libcurl');
skip_if(stripos(@file_get_contents('https://www.qq.com/'), 'tencent') === false, 'Unable to access qq.com');
skip_if(stripos(@file_get_contents('https://www.baidu.com/'), 'baidu') === false, 'Unable to access baidu.com');
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

$ch1 = curl_init();
$ch2 = curl_init();

curl_setopt($ch1, CURLOPT_URL, 'https://www.qq.com/');
curl_setopt($ch1, CURLOPT_HEADER, 0);
curl_setopt($ch1, CURLOPT_RETURNTRANSFER, 1);
curl_setopt($ch2, CURLOPT_URL, 'https://www.baidu.com/');
curl_setopt($ch2, CURLOPT_HEADER, 0);
curl_setopt($ch2, CURLOPT_RETURNTRANSFER, 1);

$response1 = curl_exec($ch1);
Assert::true(stripos($response1, 'tencent') !== false);
$response2 = curl_exec($ch2);
Assert::true(stripos($response2, 'baidu') !== false);

echo "Done\n";
?>
--EXPECT--
Done
