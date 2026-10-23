--TEST--
swow_curl: cold HTTP/2 multiplexing matches the native PHP cURL baseline
--SKIPIF--
<?php
if (getenv('SWOW_TSAN') !== false) {
    exit('skip TSan cannot handle PHP bailout');
}

require __DIR__ . '/../include/skipif.php';
require __DIR__ . '/../include/autoload.php';
Http2Server::skipIfUnavailable();
?>
--FILE--
<?php
if (($argv[1] ?? null) === '--native-modules') {
    echo json_encode(['curl' => extension_loaded('curl'), 'openssl' => extension_loaded('openssl')]);
    exit;
}
require __DIR__ . '/../include/bootstrap.php';

/** @return array{version: string, cases: list<list<int>>} */
function probeColdHttp2(): array
{
    $cases = [];
    foreach ([false, true] as $tls) {
        $server = new Http2Server($tls);
        $multi = Http2Transfer::multi();
        $transfers = [];
        try {
            for ($index = 0; $index < 2; $index++) {
                $transfer = new Http2Transfer($server, "/barrier?group=cold&count=2&id={$index}");
                Assert::true(curl_setopt($transfer->handle, CURLOPT_TIMEOUT_MS, 1000));
                $transfer->add($multi);
                $transfers[] = $transfer;
            }
            $messages = Http2Transfer::run($multi);
            Assert::same(count($messages), 2);
            $results = [];
            foreach ($messages as $message) {
                $results[spl_object_id($message['handle'])] = $message['result'];
            }
            $codes = [];
            foreach ($transfers as $index => $transfer) {
                $code = $results[spl_object_id($transfer->handle)];
                $codes[] = $code;
                if ($code === CURLE_OK) {
                    Assert::same(curl_getinfo($transfer->handle, CURLINFO_HTTP_VERSION), CURL_HTTP_VERSION_2_0);
                    Assert::same($transfer->response()['path'], "/barrier?group=cold&count=2&id={$index}");
                    Assert::same($transfer->response()['batch'], 2);
                }
                $transfer->remove($multi);
            }
            $cases[] = $codes;
        } finally {
            unset($transfer, $transfers, $messages, $message, $multi);
            gc_collect_cycles();
            $server->stop();
        }
    }
    return ['version' => curl_version()['version'], 'cases' => $cases];
}

if (($argv[1] ?? null) === '--native') {
    Assert::false(extension_loaded('swow'), 'The control must use native PHP cURL');
    echo json_encode(probeColdHttp2(), JSON_THROW_ON_ERROR);
    exit;
}

/** @param list<string> $arguments */
function runHttp2Control(array $arguments): string
{
    $process = proc_open($arguments, [0 => ['pipe', 'r'], 1 => ['pipe', 'w'], 2 => ['pipe', 'w']], $pipes);
    Assert::true(is_resource($process));
    fclose($pipes[0]);
    $output = stream_get_contents($pipes[1]);
    $errors = stream_get_contents($pipes[2]);
    fclose($pipes[1]);
    fclose($pipes[2]);
    Assert::same(proc_close($process), 0, $output . $errors);
    Assert::same($errors, '');
    return $output;
}

// Clean ini prevents the installed Swow module from entering the native control process.
Assert::true(extension_loaded('swow'));
$arguments = [PHP_BINARY, '-n', '-d', 'extension_dir=' . ini_get('extension_dir')];
$modules = json_decode(runHttp2Control([...$arguments, __FILE__, '--native-modules']), true, flags: JSON_THROW_ON_ERROR);
foreach ($modules as $module => $loaded) {
    if (!$loaded) {
        $arguments[] = '-d';
        $arguments[] = 'extension=' . $module;
    }
}
$native = json_decode(runHttp2Control([...$arguments, __FILE__, '--native']), true, flags: JSON_THROW_ON_ERROR);
$swow = probeColdHttp2();
Assert::same($swow['version'], $native['version'], 'Compare the same libcurl runtime');
Assert::same($swow['cases'], $native['cases'], 'Cold HTTP/2 completion must match native PHP cURL: native=' . json_encode($native) . ', swow=' . json_encode($swow));

echo "Done\n";
?>
--EXPECT--
Done
