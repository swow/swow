--TEST--
swow_ipaddress: common judges
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\IpAddress;

$testCases = [
    // isMappedIpv4
    ['0.0.0.0', 'isMappedIpv4', false],
    ['::ffff:0.0.0.0', 'isMappedIpv4', true],
    ['::0.0.0.0', 'isMappedIpv4', false],
    ['123d::0.0.0.0', 'isMappedIpv4', false],
    // isIpv4OrMappedIpv4
    ['0.0.0.0', 'isIpv4OrMappedIpv4', true],
    ['::ffff:0.0.0.0', 'isIpv4OrMappedIpv4', true],
    ['::0.0.0.0', 'isIpv4OrMappedIpv4', false],
    ['123d::0.0.0.0', 'isIpv4OrMappedIpv4', false],
    // isLocal
    ['1.2.3.4', 'isLocal', false],
    ['192.168.0.0', 'isLocal', true],
    ['192.168.255.255', 'isLocal', true],
    ['192.169.0.0', 'isLocal', false],
    ['172.16.0.0', 'isLocal', true],
    ['10.0.0.0', 'isLocal', true],
    ['100.100.0.0', 'isLocal', true],
    ['169.254.0.1', 'isLocal', true],
    ['::ffff:192.168.0.1', 'isLocal', true],
    ['::192.168.0.1', 'isLocal', false],
    ['fd00::192.168.0.1', 'isLocal', true],
    ['fc00::1', 'isLocal', true],
    ['fe00::1', 'isLocal', true],
    // isLoopback
    ['1.1.1.1', 'isLoopback', false],
    ['127.0.0.1', 'isLoopback', true],
    ['127.0.1.1', 'isLoopback', true],
    ['127.127.1.1', 'isLoopback', true],
    ['::1', 'isLoopback', true],
];

foreach ($testCases as $case) {
    $addr = new IpAddress($case[0]);
    Assert::eq([$addr, $case[1]](), $case[2], "'{$case[0]}'->{$case[1]}() should be" . ($case[2] ? "true" : "false"));
}

echo "Done\n";
?>
--EXPECT--
Done
