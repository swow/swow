--TEST--
swow_curl: cancel
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

use Swow\Coroutine;

foreach ([false, true] as $schedule) {
    $ch = curl_init();
    curl_setopt($ch, CURLOPT_URL, TEST_WEBSITE1_URL);
    curl_setopt($ch, CURLOPT_HEADER, 0);
    curl_setopt($ch, CURLOPT_RETURNTRANSFER, 1);
    $response = null;
    $coroutine = Coroutine::run(static function () use ($ch, &$response): void {
        $response = curl_exec($ch);
    });
    // cancel
    if ($schedule) {
        sleep(0);
    }
    $coroutine->resume();
    Assert::false($response);
}

echo "Done\n";
?>
--EXPECT--
Done
