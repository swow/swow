--TEST--
swow_curl: a clone keeps the CURLOPT_STDERR stream alive after the original is gone
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

/*
 * The clone has to take over the error stream like the other handler streams.
 * Without that it keeps libcurl writing into the FILE* of a stream that the
 * original handle released, and the verbose output of the failing transfer
 * touches freed memory.
 */
$ch = curl_init();
$stream = fopen('php://temp', 'w+');
Assert::true(curl_setopt($ch, CURLOPT_STDERR, $stream));
// verbose output goes to CURLOPT_STDERR, which is what the clone must keep
Assert::true(curl_setopt($ch, CURLOPT_VERBOSE, true));

$clone = clone $ch;

// Drop every explicit reference to the stream except the handles' own. The
// original handle releases its reference, so only the clone keeps it alive.
unset($stream, $ch);

curl_setopt($clone, CURLOPT_URL, 'http://127.0.0.1:1/');
curl_setopt($clone, CURLOPT_PROXY, '');
curl_setopt($clone, CURLOPT_TIMEOUT_MS, 1000);
Assert::false(curl_exec($clone));
// a refused connection is reported directly or as a timeout depending on the
// libcurl version, and both mean the transfer failed as intended
Assert::true(
    in_array(curl_errno($clone), [CURLE_COULDNT_CONNECT, CURLE_OPERATION_TIMEDOUT], true),
    curl_error($clone)
);

unset($clone);
echo "Done\n";

?>
--EXPECT--
Done
