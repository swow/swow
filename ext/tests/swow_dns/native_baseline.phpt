--TEST--
swow_dns: hooked DNS functions match the native PHP functions
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
skip_if_offline();
skip_if_function_not_exist('dns_get_record');
?>
--FILE--
<?php
/**
 * Runs the same DNS calls in a native PHP process (without Swow) and in this process (with Swow
 * hooks), then compares return values, warnings and exceptions. TTLs are dropped because they
 * count down between the two runs, and record lists are sorted because resolvers may rotate them.
 */
function normalizeDnsResult(mixed $value): mixed
{
    if (!is_array($value)) {
        return $value;
    }
    unset($value['ttl']);
    // PHP on Windows returns the in-memory DNS_RECORD data for raw queries, which holds pointers
    // that differ in every process; only its length is comparable there
    if (PHP_OS_FAMILY === 'Windows' && is_int($value['type'] ?? null) && is_string($value['data'] ?? null)) {
        $value['data'] = strlen($value['data']);
    }
    $value = array_map('normalizeDnsResult', $value);
    if (array_is_list($value)) {
        usort($value, static fn(mixed $a, mixed $b): int => strcmp(serialize($a), serialize($b)));
    }
    return $value;
}

/**
 * DNS_ANY answers and the authority and additional sections (the additional one also comes from a
 * DNS_ANY query) are chosen by the resolver for each query, so even the record types differ between
 * two runs. What stays comparable is that every record follows PHP's field layout for its type;
 * this returns the records that do not.
 *
 * @param list<array<string, mixed>> $records
 * @return list<array<string, mixed>>
 */
function malformedDnsRecords(array $records): array
{
    $fields = [
        'A' => ['ip'],
        'AAAA' => ['ipv6'],
        'MX' => ['pri', 'target'],
        'CNAME' => ['target'],
        'NS' => ['target'],
        'PTR' => ['target'],
        'HINFO' => ['cpu', 'os'],
        'CAA' => ['flags', 'tag', 'value'],
        'TXT' => ['txt', 'entries'],
        'SOA' => ['mname', 'rname', 'serial', 'refresh', 'retry', 'expire', 'minimum-ttl'],
        'A6' => ['masklen', 'ipv6'],
        'SRV' => ['pri', 'weight', 'port', 'target'],
        'NAPTR' => ['order', 'pref', 'flags', 'services', 'regex', 'replacement'],
    ];
    return array_values(array_filter($records, static function (array $record) use ($fields): bool {
        $type = $record['type'] ?? null;
        // raw mode reports the numeric type with the undecoded data
        $expected = is_int($type) ? ['data'] : ($fields[$type] ?? null);
        if ($expected === null) {
            return true;
        }
        $expected = [...['host', 'class', 'ttl', 'type'], ...$expected];
        if ($type === 'A6' && array_key_exists('chain', $record)) {
            $expected[] = 'chain';
        }
        $keys = array_keys($record);
        sort($keys);
        sort($expected);
        return $keys !== $expected;
    }));
}

/** @return array{result: mixed, warnings: list<string>, error: list<string>|null} */
function probeDnsCase(callable $case): array
{
    $warnings = [];
    set_error_handler(static function (int $errno, string $message) use (&$warnings): bool {
        $warnings[] = $message;
        return true;
    });
    $error = null;
    try {
        $result = $case();
    } catch (Throwable $throwable) {
        $result = null;
        $error = [$throwable::class, $throwable->getMessage()];
    } finally {
        restore_error_handler();
    }
    return ['result' => normalizeDnsResult($result), 'warnings' => $warnings, 'error' => $error];
}

/** @return array<string, array{result: mixed, warnings: list<string>, error: list<string>|null}> */
function probeDns(): array
{
    // php-cs-fixer rewrites calls of alias names, call them through variables so that both hooked
    // function table entries are covered
    $dnsGetMx = 'dns_get_mx';
    $dnsCheckRecord = 'dns_check_record';

    $cases = [
        // gethostbynamel()
        'gethostbynamel localhost' => static fn() => gethostbynamel('localhost'),
        'gethostbynamel ip literal' => static fn() => gethostbynamel('127.0.0.1'),
        'gethostbynamel domain' => static fn() => gethostbynamel('php.net'),
        'gethostbynamel nxdomain' => static fn() => gethostbynamel('nonexistent.invalid'),
        'gethostbynamel empty' => static fn() => gethostbynamel(''),
        'gethostbynamel too long' => static fn() => gethostbynamel(str_repeat('a', 256)),
        // gethostbyaddr()
        'gethostbyaddr ipv4 loopback' => static fn() => gethostbyaddr('127.0.0.1'),
        'gethostbyaddr ipv6 loopback' => static fn() => gethostbyaddr('::1'),
        'gethostbyaddr ptr' => static fn() => gethostbyaddr('8.8.8.8'),
        'gethostbyaddr no ptr' => static fn() => gethostbyaddr('192.0.2.1'),
        'gethostbyaddr invalid' => static fn() => gethostbyaddr('not-an-ip'),
        // dns_get_record()
        'dns_get_record A' => static fn() => dns_get_record('php.net', DNS_A),
        'dns_get_record AAAA' => static fn() => dns_get_record('php.net', DNS_AAAA),
        'dns_get_record MX' => static fn() => dns_get_record('php.net', DNS_MX),
        'dns_get_record NS' => static fn() => dns_get_record('php.net', DNS_NS),
        'dns_get_record SOA' => static fn() => dns_get_record('php.net', DNS_SOA),
        'dns_get_record TXT' => static fn() => dns_get_record('php.net', DNS_TXT),
        'dns_get_record CNAME' => static fn() => dns_get_record('www.github.com', DNS_CNAME),
        'dns_get_record PTR' => static fn() => dns_get_record('8.8.8.8.in-addr.arpa', DNS_PTR),
        'dns_get_record SRV' => static fn() => dns_get_record('_imaps._tcp.gmail.com', DNS_SRV),
        'dns_get_record mask' => static fn() => dns_get_record('php.net', DNS_A | DNS_MX | DNS_TXT),
        // DNS_ANY answers are a resolver-chosen subset that changes between queries (RFC 8482),
        // so only their record layout is comparable
        'dns_get_record ANY' => static function (): array|false {
            $records = dns_get_record('php.net', DNS_ANY);
            return $records === false ? false : ['malformed' => malformedDnsRecords($records)];
        },
        'dns_get_record default' => static function (): array|false {
            $records = dns_get_record('php.net');
            return $records === false ? false : ['malformed' => malformedDnsRecords($records)];
        },
        'dns_get_record raw' => static fn() => dns_get_record('php.net', 15, raw: true),
        'dns_get_record sections' => static function (): array {
            $records = dns_get_record('php.net', DNS_MX, $authns, $addtl);
            return ['records' => $records, 'malformed authns' => malformedDnsRecords($authns), 'malformed addtl' => malformedDnsRecords($addtl)];
        },
        'dns_get_record raw sections' => static function (): array {
            $records = dns_get_record('php.net', 2, $authns, $addtl, true);
            return ['records' => $records, 'malformed authns' => malformedDnsRecords($authns), 'malformed addtl' => malformedDnsRecords($addtl)];
        },
        'dns_get_record nxdomain' => static fn() => dns_get_record('nonexistent.invalid', DNS_A),
        'dns_get_record invalid type' => static fn() => dns_get_record('php.net', 0x7),
        'dns_get_record raw type too small' => static fn() => dns_get_record('php.net', 0, raw: true),
        'dns_get_record raw type too large' => static fn() => dns_get_record('php.net', 0x10000, raw: true),
        // dns_get_mx() and getmxrr()
        'dns_get_mx' => static function () use ($dnsGetMx): array {
            $result = $dnsGetMx('php.net', $hosts, $weights);
            return [$result, array_map(null, $hosts, $weights)];
        },
        'getmxrr without weights' => static function (): array {
            $result = getmxrr('php.net', $hosts);
            return [$result, $hosts];
        },
        'getmxrr nxdomain' => static function (): array {
            $result = getmxrr('nonexistent.invalid', $hosts, $weights);
            return [$result, $hosts, $weights];
        },
        // dns_check_record() and checkdnsrr()
        'dns_check_record default' => static fn() => $dnsCheckRecord('php.net'),
        'dns_check_record empty' => static fn() => $dnsCheckRecord(''),
        'checkdnsrr default' => static fn() => checkdnsrr('php.net'),
        'checkdnsrr A' => static fn() => checkdnsrr('php.net', 'A'),
        'checkdnsrr lowercase type' => static fn() => checkdnsrr('php.net', 'txt'),
        'checkdnsrr missing record' => static fn() => checkdnsrr('www.github.com', 'MX'),
        'checkdnsrr nxdomain' => static fn() => checkdnsrr('nonexistent.invalid', 'A'),
        'checkdnsrr empty' => static fn() => checkdnsrr(''),
        'checkdnsrr invalid type' => static fn() => checkdnsrr('php.net', 'INVALID'),
    ];
    if (defined('DNS_CAA')) {
        $cases['dns_get_record CAA'] = static fn() => dns_get_record('google.com', DNS_CAA);
        $cases['checkdnsrr CAA'] = static fn() => checkdnsrr('google.com', 'CAA');
    }

    $results = [];
    foreach ($cases as $name => $case) {
        $results[$name] = probeDnsCase($case);
    }
    return $results;
}

if (($argv[1] ?? null) === '--native') {
    if (extension_loaded('swow')) {
        fwrite(STDERR, 'The control must use native PHP DNS functions');
        exit(1);
    }
    echo json_encode(probeDns(), JSON_THROW_ON_ERROR | JSON_INVALID_UTF8_SUBSTITUTE);
    exit;
}

require __DIR__ . '/../include/bootstrap.php';

// Clean ini prevents the installed Swow module from entering the native control process.
Assert::true(extension_loaded('swow'));
$process = proc_open([PHP_BINARY, '-n', __FILE__, '--native'], [0 => ['pipe', 'r'], 1 => ['pipe', 'w'], 2 => ['pipe', 'w']], $pipes);
Assert::true(is_resource($process));
fclose($pipes[0]);
$output = stream_get_contents($pipes[1]);
$errors = stream_get_contents($pipes[2]);
fclose($pipes[1]);
fclose($pipes[2]);
Assert::same(proc_close($process), 0, $output . $errors);
Assert::same($errors, '');
$native = json_decode($output, true, flags: JSON_THROW_ON_ERROR);
$swow = json_decode(json_encode(probeDns(), JSON_THROW_ON_ERROR | JSON_INVALID_UTF8_SUBSTITUTE), true, flags: JSON_THROW_ON_ERROR);

Assert::same(array_keys($swow), array_keys($native));
foreach ($native as $name => $expected) {
    Assert::same($swow[$name], $expected, "{$name}: native=" . json_encode($expected) . ', swow=' . json_encode($swow[$name]));
}

echo "Done\n";
?>
--EXPECT--
Done
