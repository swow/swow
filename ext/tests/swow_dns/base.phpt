--TEST--
swow_dns: basic functionality
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
skip_if_offline();
if (PHP_OS_FAMILY === 'Windows' || PHP_OS_FAMILY === 'WinNT' || PHP_OS_FAMILY === 'Cygwin') {
    // for windows, there's a bug
    $enterpriseclientEnabled = `netsh interface teredo show state`;
    skip_if(!strstr($enterpriseclientEnabled, 'enterpriseclient'), "teredo not enabled, gai may fail");
}
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

// dual stack
Assert::notSame(gethostbyname('www.apple.com'), 'www.apple.com');
// ipv6 only
Assert::same(gethostbyname('6.donot.help'), '6.donot.help');
// ipv6 only
Assert::notSame(gethostbyname_ex('6.donot.help', AF_UNSPEC), '6.donot.help');
Assert::notSame(gethostbyname_ex('6.donot.help', AF_INET6), '6.donot.help');
Assert::throws(function () {
    gethostbyname_ex('6.donot.help', AF_INET);
}, RuntimeException::class);
// ipv4 only
Assert::notSame(gethostbyname_ex('4.donot.help', AF_UNSPEC), '4.donot.help');
Assert::throws(function () {
    gethostbyname_ex('4.donot.help', AF_INET6);
}, RuntimeException::class);
Assert::notSame(gethostbyname_ex('4.donot.help', AF_INET), '4.donot.help');
// dual stack
Assert::notSame(gethostbyname_ex('dualstack.donot.help', AF_UNSPEC), 'dualstack.donot.help');
Assert::notSame(gethostbyname_ex('dualstack.donot.help', AF_INET6), 'dualstack.donot.help');
Assert::notSame(gethostbyname_ex('dualstack.donot.help', AF_INET), 'dualstack.donot.help');

echo "Done\n";
?>
--EXPECT--
Done
