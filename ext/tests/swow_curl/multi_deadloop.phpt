--TEST--
swow_curl: multi handle deadloop
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
skip_if(!Swow\Extension::isBuiltWith('curl'), 'extension must be built with libcurl');
require __DIR__ . '/../include/bootstrap.php';
skip_if(!str_contains(@file_get_contents(TEST_WEBSITE2_URL), TEST_WEBSITE2_KEYWORD), 'Unable to access ' . TEST_WEBSITE2_URL);
skip_if(memory_get_usage() === 0, 'needs zend mm');
?>
--FILE--
<?php
require_once __DIR__ . '/../include/bootstrap.php';

$ch1 = curl_init();
curl_setopt($ch1, CURLOPT_URL, TEST_WEBSITE1_URL);
curl_setopt($ch1, CURLOPT_RETURNTRANSFER, 1);

$ch2 = curl_init();
curl_setopt($ch2, CURLOPT_URL, TEST_WEBSITE2_URL);
curl_setopt($ch2, CURLOPT_RETURNTRANSFER, 1);

$mh = curl_multi_init();
curl_multi_add_handle($mh, $ch1);
curl_multi_add_handle($mh, $ch2);

$running = null;
curl_multi_exec($mh, $running);

$startMemory = memory_get_usage();

sleep(3);

Assert::lessThan(memory_get_usage() - $startMemory, 1024 * 1024);
echo "Done\n";
?>
--EXPECT--
Done
