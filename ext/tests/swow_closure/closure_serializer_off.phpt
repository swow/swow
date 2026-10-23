--TEST--
swow_closure: serialization is disabled by default
--SKIPIF--
<?php
$setting = ini_get('swow.closure_serializer');
if ($setting !== false && $setting !== '' && $setting !== '0' && strcasecmp($setting, 'off') !== 0) {
    die('skip swow.closure_serializer is enabled');
}
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

try {
    serialize(static function (): void {
    });
} catch (Throwable $e) {
    echo get_class($e), ': ', $e->getMessage(), "\n";
}
?>
--EXPECT--
Exception: Serialization of 'Closure' is not allowed
