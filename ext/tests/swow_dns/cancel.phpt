--TEST--
swow_dns: hooked DNS functions handle wait timeout and coroutine cancellation
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
use Swow\Socket;

// php-cs-fixer rewrites calls of alias names, call them through variables so that both hooked
// function table entries are covered
$dnsGetMx = 'dns_get_mx';
$dnsCheckRecord = 'dns_check_record';

/** @return array{mixed, list<string>} */
function callWithWarnings(callable $callable): array
{
    $warnings = [];
    set_error_handler(static function (int $errno, string $message) use (&$warnings): bool {
        $warnings[] = $message;
        return true;
    });
    try {
        return [$callable(), $warnings];
    } finally {
        restore_error_handler();
    }
}

// A zero DNS timeout gives up before the thread pool can answer; each function falls back to its
// own failure value and the record functions report why.
Socket::setGlobalDnsTimeout(0);
try {
    [$result, $warnings] = callWithWarnings(static fn() => gethostbynamel('php.net'));
    Assert::false($result);
    Assert::same($warnings, []);
    [$result, $warnings] = callWithWarnings(static fn() => gethostbyaddr('127.0.0.1'));
    Assert::same($result, '127.0.0.1');
    Assert::same($warnings, []);
    $cases = [
        'dns_get_record' => static fn() => dns_get_record('php.net', DNS_MX),
        'dns_get_mx' => static fn() => $dnsGetMx('php.net', $hosts),
        'getmxrr' => static fn() => getmxrr('php.net', $hosts),
        'dns_check_record' => static fn() => $dnsCheckRecord('php.net'),
        'checkdnsrr' => static fn() => checkdnsrr('php.net'),
    ];
    foreach ($cases as $name => $case) {
        [$result, $warnings] = callWithWarnings($case);
        Assert::false($result, $name);
        Assert::same(count($warnings), 1, $name);
        Assert::true(str_starts_with($warnings[0], "{$name}(): DNS query wait failed: "), $warnings[0]);
    }
} finally {
    Socket::setGlobalDnsTimeout(-1);
}

// Killing coroutines while their queries are still running in the thread pool must not break
// later queries; the abandoned queries are released when the worker threads finish.
$coroutines = [];
foreach ([
    static fn() => dns_get_record('php.net', DNS_MX),
    static fn() => $dnsGetMx('php.net', $hosts),
    static fn() => $dnsCheckRecord('php.net'),
    static fn() => gethostbyaddr('8.8.8.8'),
] as $case) {
    $coroutines[] = Coroutine::run($case);
}
foreach ($coroutines as $coroutine) {
    $coroutine->kill();
    Assert::false($coroutine->isAvailable());
}
Assert::notSame(dns_get_record('php.net', DNS_MX), false);
Assert::true(checkdnsrr('php.net'));

echo "Done\n";
?>
--EXPECT--
Done
