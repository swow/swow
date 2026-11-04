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
    // a zero mask must not shift by the width of the type
    ['1.2.3.4', '0.0.0.0/0', true],
    ['255.255.255.255', '0.0.0.0/0', true],
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
    // a mask that ends inside a component must compare the remaining bits
    ['fd00::1', 'fd00::/33', true],
    ['fd00:0:8000::1', 'fd00::/33', false],
    ['fd00::1', 'fd00::/65', true],
    ['fd00:0:0:0:8000::1', 'fd00::/65', false],
    ['fd00:0:0:0:0:0:0:1', 'fd00::/127', true],
    ['fd00:0:0:0:0:0:0:2', 'fd00::/127', false],
    ['fd00:0:0:0:0:0:0:3', 'fd00::/126', true],
    ['fd00:0:0:0:0:0:0:4', 'fd00::/126', false],
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
