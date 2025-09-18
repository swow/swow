--TEST--
swow_curl: multi (SSL connection)
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

use Swow\Http\Status;

// create the multiple cURL handle
$mh = curl_multi_init();

for ($n = 2; $n--;) {
    // create both cURL resources
    $ch1 = curl_init();
    $ch2 = curl_init();

    // set URL and other appropriate options
    curl_setopt($ch1, CURLOPT_URL, TEST_WEBSITE1_URL);
    curl_setopt($ch1, CURLOPT_HEADER, 0);
    curl_setopt($ch1, CURLOPT_RETURNTRANSFER, 1);
    curl_setopt($ch2, CURLOPT_URL, TEST_WEBSITE2_URL);
    curl_setopt($ch2, CURLOPT_HEADER, 0);
    curl_setopt($ch2, CURLOPT_RETURNTRANSFER, 1);

    // add the two handles
    curl_multi_add_handle($mh, $ch1);
    curl_multi_add_handle($mh, $ch2);

    // execute the multi handle
    do {
        $status = curl_multi_exec($mh, $active);
        if ($active) {
            $r = curl_multi_select($mh);
            Assert::greaterThanEq($r, 0);
        }
    } while ($active && $status === CURLM_OK);

    Assert::eq(curl_getinfo($ch1, CURLINFO_HTTP_CODE), Status::OK);
    Assert::eq(curl_getinfo($ch2, CURLINFO_HTTP_CODE), Status::OK);
    $response1 = curl_multi_getcontent($ch1);
    $response2 = curl_multi_getcontent($ch2);

    // close the handles
    curl_multi_remove_handle($mh, $ch1);
    if (PHP_VERSION_ID < 80100) {
        curl_close($ch1);
    }
    curl_multi_remove_handle($mh, $ch2);
    if (PHP_VERSION_ID < 80100) {
        curl_close($ch2);
    }
    curl_multi_close($mh);

    Assert::contains($response1, TEST_WEBSITE1_KEYWORD);
    Assert::contains($response2, TEST_WEBSITE2_KEYWORD);
}

echo "Done\n";
?>
--EXPECT--
Done
