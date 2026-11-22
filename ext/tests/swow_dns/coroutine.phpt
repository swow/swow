--TEST--
swow_dns: hooked DNS functions only suspend the calling coroutine
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
skip_if_offline();
skip_if_function_not_exist('dns_get_record');
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Coroutine;
use Swow\Sync\WaitReference;

// php-cs-fixer rewrites calls of alias names, call them through variables so that both hooked
// function table entries are covered
$dnsGetMx = 'dns_get_mx';
$dnsCheckRecord = 'dns_check_record';

$cases = [
    'gethostbynamel' => static fn() => gethostbynamel('localhost'),
    'gethostbyaddr' => static fn() => gethostbyaddr('127.0.0.1'),
    'dns_get_record' => static fn() => dns_get_record('php.net', DNS_MX),
    'dns_get_mx' => static fn() => $dnsGetMx('php.net', $hosts),
    'getmxrr' => static fn() => getmxrr('php.net', $hosts),
    'dns_check_record' => static fn() => $dnsCheckRecord('php.net', 'MX'),
    'checkdnsrr' => static fn() => checkdnsrr('php.net', 'MX'),
];

// Coroutine::run() returns at the first suspension of the new coroutine, a blocking call would
// have finished before returning.
$wr = new WaitReference();
$results = [];
foreach ($cases as $name => $case) {
    Coroutine::run(static function () use ($name, $case, &$results, $wr): void {
        $results[$name] = $case();
    });
    Assert::false(isset($results[$name]), "{$name}() must suspend the coroutine instead of blocking");
}
WaitReference::wait($wr);
Assert::same(count($results), count($cases));
foreach ($results as $name => $result) {
    Assert::notSame($result, false, "{$name}() failed");
}

// Concurrent record queries keep other coroutines running.
$ticks = 0;
$ticker = Coroutine::run(static function () use (&$ticks): void {
    while (true) {
        usleep(1000);
        $ticks++;
    }
});
$wr = new WaitReference();
$records = [];
for ($i = 0; $i < 8; $i++) {
    Coroutine::run(static function () use ($i, &$records, $wr): void {
        $records[$i] = dns_get_record('php.net', DNS_A | DNS_MX | DNS_TXT);
    });
}
WaitReference::wait($wr);
$ticker->kill();
Assert::same(count($records), 8);
foreach ($records as $record) {
    Assert::notSame($record, false);
}
Assert::greaterThan($ticks, 0);

echo "Done\n";
?>
--EXPECT--
Done
