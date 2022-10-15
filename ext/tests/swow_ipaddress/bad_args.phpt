--TEST--
swow_ipaddress: bad args
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\IpAddress;

$testCases = [
    // domain
    ['0x07.cc'],
    ['toast.run'],
    // too long
    ['cafe::babe:babe:babe:babe:babe:babe:babe:babe'],
];

foreach ($testCases as $case) {
    //new IpAddress($case[0]);
    Assert::throws(function () use ($case) {
        $addr = new IpAddress($case[0]);
    }, Swow\IpAddressException::class);
    $addr = new IpAddress();
    Assert::throws(function () use ($addr, $case) {
        $addr->setIp($case[0]);
    }, Swow\IpAddressException::class);
}

// bad port
$addr = new IpAddress('::1');
$addr->setPort(80);

Assert::throws(function () use ($addr) {
    $addr->setPort(-1);
}, ValueError::class);

Assert::throws(function () use ($addr) {
    $addr->setPort(1048576);
}, ValueError::class);

// bad maskLen
Assert::throws(function () use ($addr) {
    $addr->setMaskLen(-1);
}, ValueError::class);

Assert::throws(function () use ($addr) {
    $addr->setMaskLen(129);
}, ValueError::class);

$addr->setFlags(IpAddress::IPV4);

Assert::throws(function () use ($addr) {
    $addr->setMaskLen(-1);
}, ValueError::class);

Assert::throws(function () use ($addr) {
    $addr->setMaskLen(33);
}, ValueError::class);

echo "Done\n";
?>
--EXPECT--
Done
