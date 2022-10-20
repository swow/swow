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
use Swow\IpAddressException;

$testCases = [
    // domain
    ['0x07.cc'],
    ['toast.run'],
    // too long
    ['cafe::babe:babe:babe:babe:babe:babe:babe:babe'],
];

foreach ($testCases as $case) {
    // new IpAddress($case[0]);
    Assert::throws(static function () use ($case): void {
        $addr = new IpAddress($case[0]);
    }, IpAddressException::class);
    $addr = new IpAddress();
    Assert::throws(static function () use ($addr, $case): void {
        $addr->setIp($case[0]);
    }, IpAddressException::class);
}

// bad port
$addr = new IpAddress('::1');
$addr->setPort(80);

Assert::throws(static function () use ($addr): void {
    $addr->setPort(-1);
}, ValueError::class);

Assert::throws(static function () use ($addr): void {
    $addr->setPort(1048576);
}, ValueError::class);

// bad maskLen
Assert::throws(static function () use ($addr): void {
    $addr->setMaskLen(-1);
}, ValueError::class);

Assert::throws(static function () use ($addr): void {
    $addr->setMaskLen(129);
}, ValueError::class);

$addr->setFlags(IpAddress::IPV4);

Assert::throws(static function () use ($addr): void {
    $addr->setMaskLen(-1);
}, ValueError::class);

Assert::throws(static function () use ($addr): void {
    $addr->setMaskLen(33);
}, ValueError::class);

$addr = new IpAddress('1.2.3.4');
$cidr = new IpAddress('1.0.0.0/20');
$cidr6 = new IpAddress('::/20');

Assert::throws(static function () use ($addr, $cidr): void {
    $cidr->in($addr);
}, IpAddressException::class);

Assert::throws(static function () use ($addr, $cidr): void {
    $addr->covers($cidr);
}, IpAddressException::class);

Assert::throws(static function () use ($addr, $cidr6): void {
    $addr->in($cidr6);
}, IpAddressException::class);

Assert::throws(static function () use ($addr, $cidr6): void {
    $cidr6->covers($addr);
}, IpAddressException::class);

echo "Done\n";
?>
--EXPECT--
Done
