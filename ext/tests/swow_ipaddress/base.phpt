--TEST--
swow_ipaddress: basic functionality
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\IpAddress;

$testCases = [
    ['255.255.255.255', '255.255.255.255', '255.255.255.255', 0, 0],
    ['127.0.0.1', '127.0.0.1', '127.0.0.1', 0, 0],
    ['0.0.0.0', '0.0.0.0', '0.0.0.0', 0, 0],
    ['0.0.0.0:23333', '0.0.0.0', '0.0.0.0', 23333, 0],
    ['192.168.0.0/16', '192.168.0.0/16', '192.168.0.0', 0, 16],
    ['fd00::', 'fd00::', 'fd00::', 0, 0],
    ['::', '::', '::', 0, 0],
    ['::1', '::1', '::1', 0, 0],
    ['::ffff:1.2.3.4', '::ffff:1.2.3.4', '::ffff:102:304', 0, 0],
    ['::5.6.7.8', '::5.6.7.8', '::506:708', 0, 0],
    ['cafe:babe:dead:beef::', 'cafe:babe:dead:beef::', 'cafe:babe:dead:beef::', 0, 0],
    ['cafe:babe:dead:beef:f00f:c7c8:9090:9090', 'cafe:babe:dead:beef:f00f:c7c8:9090:9090', 'cafe:babe:dead:beef:f00f:c7c8:9090:9090', 0, 0],
    ['::/64', '::/64', '::', 0, 64],
    ['[::]:123', '::', '::', 123, 0],
    ['0:0:0:0000:000::1', '::1', '::1', 0, 0],
    ['fd00:0:0:0000:000::1', 'fd00::1', 'fd00::1', 0, 0],
];

foreach ($testCases as $case) {
    $addr = new IpAddress($case[0]);
    Assert::same($addr->getFull(), $case[1]);
    Assert::same($addr->getIp(), $case[2]);
    Assert::same($addr->getPort(), $case[3]);
    Assert::same($addr->getMaskLen(), $case[4]);
}
foreach ($testCases as $case) {
    $addr = new IpAddress();
    Assert::same($addr->getIp(), '::');
    Assert::same($addr->getPort(), 0);
    Assert::same($addr->getMaskLen(), 0);
    $addr->setFull($case[0]);
    Assert::same($addr->getFull(), $case[1]);
    $addr->setIp($case[2]);
    Assert::same($addr->getIp(), $case[2]);
    Assert::same($addr->setPort(123)->getPort(), 123);
    Assert::same($addr->setMaskLen(12)->getMaskLen(), 12);
    if (!($addr->getFlags() & IpAddress::IPV4)) {
        Assert::same($addr->setMaskLen(126)->getMaskLen(), 126);
    }
}

echo "Done\n";
?>
--EXPECT--
Done
