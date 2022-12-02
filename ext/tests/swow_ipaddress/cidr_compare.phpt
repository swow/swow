--TEST--
swow_ipaddress: cidr compare
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\IpAddress;

$testCases = [
    ['0.0.0.0', '0.0.0.0/1', true],
    ['0.0.0.0', '0.0.0.0/32', true],
    ['1.0.0.0', '0.0.0.0/8', false],
    ['0.0.0.0', '0.0.0.1/8', true],
    ['0.0.0.0/7', '0.0.0.1/8', false],
    ['0.0.0.2/8', '0.0.0.1/8', true],
    ['fd00::1', 'fd00::/16', true],
    ['fd00:1234::1', 'fd00:1234::/32', true],
    ['fd00:1234::123:23:11', 'fd00:1234::/36', true],
    ['fd00:1234:1::123:23:11', 'fd00:1234:1::/56', true],
    ['fd00:1234:1::123:23:11', 'fd00:1234::/56', false],
    ['2001::1', 'fd00::/16', false],
    ['fd00::', 'fd00::/128', true],
    ['fd00::', 'fd00::/0', true],
];

foreach ($testCases as $case) {
    $addr = new IpAddress($case[0]);
    $cidr = new IpAddress($case[1]);
    Assert::same($addr->in($cidr), $case[2], "'{$case[0]}'->in('{$case[1]}') should be " . ($case[2] ? 'true' : 'false'));
    Assert::same($cidr->covers($addr), $case[2], "'{$case[1]}'->covers('{$case[0]}') should be " . ($case[2] ? 'true' : 'false'));
}

echo "Done\n";
?>
--EXPECT--
Done
