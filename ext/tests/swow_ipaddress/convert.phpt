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
use Swow\IpAddressException;

$v4 = new IpAddress("1.2.3.4");

$v4->convertToMappedIpv4();

Assert::same($v4->getFlags(), IpAddress::IPV4_EMBED);
Assert::same($v4->getMaskLen(), 0);
Assert::same($v4->getPort(), 0);

$v4->setFull("1.2.3.4:1234");
$v4->convertToMappedIpv4();

Assert::same($v4->getFlags(), IpAddress::IPV4_EMBED | IpAddress::HAS_PORT);
Assert::same($v4->getMaskLen(), 0);
Assert::same($v4->getPort(), 1234);

$v4->setFull("1.2.3.4/12");
$v4->convertToMappedIpv4();

Assert::same($v4->getFlags(), IpAddress::IPV4_EMBED | IpAddress::HAS_MASK);
Assert::same($v4->getMaskLen(), 108);
Assert::same($v4->getPort(), 0);

$v4->setFull("::");
Assert::throws(function () use ($v4) {
    $v4->convertToMappedIpv4();
}, IpAddressException::class);

$v6 = new IpAddress("::ffff:1.2.3.4");

$v6->convertToIpv4();

Assert::same($v6->getFlags(), IpAddress::IPV4);
Assert::same($v6->getMaskLen(), 0);
Assert::same($v6->getPort(), 0);

$v6->setFull("[::ffff:1.2.3.4]:1234");
$v6->convertToIpv4();

Assert::same($v6->getFlags(), IpAddress::IPV4 | IpAddress::HAS_PORT);
Assert::same($v6->getMaskLen(), 0);
Assert::same($v6->getPort(), 1234);

$v6->setFull("::ffff:1.2.3.4/90");
$v6->convertToIpv4();

Assert::same($v6->getFlags(), IpAddress::IPV4 | IpAddress::HAS_MASK);
Assert::same($v6->getMaskLen(), 0);
Assert::same($v6->getPort(), 0);

$v6->setFull("::ffff:1.2.3.4/108");
$v6->convertToIpv4();

Assert::same($v6->getFlags(), IpAddress::IPV4 | IpAddress::HAS_MASK);
Assert::same($v6->getMaskLen(), 12);
Assert::same($v6->getPort(), 0);

$v6->setFull("::1.2.3.4");

Assert::throws(function () use ($v6) {
    $v6->convertToIpv4();
}, IpAddressException::class);

Assert::throws(function () use ($v6) {
    $v6->convertToIpv4(false);
}, IpAddressException::class);

$v6->convertToIpv4(true);
Assert::same($v6->getFlags(), IpAddress::IPV4);
Assert::same($v6->getMaskLen(), 0);
Assert::same($v6->getPort(), 0);

echo "Done\n";
?>
--EXPECT--
Done
