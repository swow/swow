--TEST--
swow_curl: multi (SSL connection)
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

use Swow\Http\Status;

// create the multiple cURL handle
$mh = curl_multi_init();

for ($n = 2; $n--;) {
    // create both cURL resources
    $ch1 = curl_init();
    $ch2 = curl_init();

    // set URL and other appropriate options
    curl_setopt($ch1, CURLOPT_URL, 'https://www.qq.com/');
    curl_setopt($ch1, CURLOPT_HEADER, 0);
    curl_setopt($ch1, CURLOPT_RETURNTRANSFER, 1);
    curl_setopt($ch2, CURLOPT_URL, 'https://www.baidu.com/');
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
    Assert::true(stripos($response1, 'tencent') !== false);
    $response2 = curl_multi_getcontent($ch2);
    Assert::true(stripos($response2, 'baidu') !== false);

    // close the handles
    curl_multi_remove_handle($mh, $ch1);
    curl_close($ch1);
    curl_multi_remove_handle($mh, $ch2);
    curl_close($ch2);
    curl_multi_close($mh);
}

echo "Done\n";
?>
--EXPECT--
Done
