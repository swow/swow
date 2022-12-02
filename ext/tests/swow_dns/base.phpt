--TEST--
swow_dns: basic functionality
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
skip_if_offline();
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

// dual stack
Assert::notSame(gethostbyname('www.apple.com'), 'www.apple.com');
// ipv4 only
Assert::notSame(gethostbyname2('4.donot.help', AF_UNSPEC), '4.donot.help');
if (PHP_OS_FAMILY !== 'Darwin') {
    // macos specified bug: you may get a mapped v4 for A-record-only domain
    Assert::throws(static function (): void {
        gethostbyname2('4.donot.help', AF_INET6);
    }, RuntimeException::class);
}
Assert::notSame(gethostbyname2('4.donot.help', AF_INET), '4.donot.help');
// dual stack
Assert::notSame(gethostbyname2('dualstack.donot.help', AF_UNSPEC), 'dualstack.donot.help');
Assert::notSame(gethostbyname2('dualstack.donot.help', AF_INET), 'dualstack.donot.help');

echo "Done\n";
?>
--EXPECT--
Done
