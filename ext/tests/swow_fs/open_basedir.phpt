--TEST--
swow_fs: file wrapper operations respect open_basedir and retain allowed files
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
skip_if(!is_writable(__DIR__), 'test directory is not writable');
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

$root = __DIR__ . '/open_basedir_fixture';
$allowed = $root . '/allowed';
$denied = $root . '/denied';
$source = $allowed . '/file';
$outside = $denied . '/file';
Assert::true(mkdir($allowed, 0700, true));
Assert::true(mkdir($denied));
Assert::same(file_put_contents($source, 'allowed'), 7);
Assert::same(file_put_contents($outside, 'denied'), 6);
Assert::notSame(ini_set('open_basedir', $allowed), false);

/** @param Closure(): mixed $operation */
$checkDenied = static function (Closure $operation): void {
    $warnings = [];
    set_error_handler(static function (int $severity, string $message) use (&$warnings): bool {
        Assert::same($severity, E_WARNING);
        $warnings[] = $message;
        return true;
    });
    try {
        Assert::false($operation());
    } finally {
        restore_error_handler();
    }
    Assert::notEmpty($warnings);
    Assert::contains(implode("\n", $warnings), 'open_basedir restriction in effect');
};
foreach ([
    static fn(): mixed => fopen($outside, 'rb'),
    static fn(): mixed => fopen('file://' . $outside, 'rb'),
    static fn(): string|false => file_get_contents($outside),
    static fn(): int|false => file_put_contents($outside, 'changed'),
    static fn(): array|false => stat($outside),
    static fn(): array|false => lstat($outside),
    static fn(): bool => file_exists($outside),
    static fn(): bool => file_exists('file://' . $outside),
    static fn(): string|false => realpath($outside),
    static fn(): bool => touch($outside),
    static fn(): bool => chmod($outside, 0600),
    static fn(): bool => unlink($outside),
    static fn(): bool => rename($source, $denied . '/renamed'),
    static fn(): bool => rename($outside, $allowed . '/renamed'),
    static fn(): bool => copy($source, $denied . '/copied'),
    static fn(): bool => copy($outside, $allowed . '/copied'),
    static fn(): mixed => opendir($denied),
    static fn(): bool => mkdir($denied . '/new'),
    static fn(): bool => rmdir($denied),
] as $operation) {
    $checkDenied($operation);
}
Assert::same(file_get_contents($source), 'allowed');
Assert::true(rename($source, $allowed . '/renamed'));
Assert::same(file_get_contents($allowed . '/renamed'), 'allowed');
Assert::true(unlink($allowed . '/renamed'));
echo "Done\n";
?>
--CLEAN--
<?php
// Cleanup runs in a fresh process: the test's tightened access policy cannot be relaxed in-process.
$root = __DIR__ . '/open_basedir_fixture';
foreach (['allowed', 'denied'] as $name) {
    $directory = $root . '/' . $name;
    if (is_dir($directory)) {
        foreach (scandir($directory) as $entry) {
            if ($entry !== '.' && $entry !== '..') {
                unlink($directory . '/' . $entry);
            }
        }
        rmdir($directory);
    }
}
if (is_dir($root)) {
    rmdir($root);
}
?>
--EXPECT--
Done
