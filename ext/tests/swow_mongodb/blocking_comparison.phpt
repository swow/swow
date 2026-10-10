--TEST--
swow_mongodb: only the MongoDB hook changes whether an in-flight query blocks other coroutines
--SKIPIF--
<?php
require __DIR__ . '/skipif.inc';
skip_if(!is_file(dirname(__DIR__, 3) . '/benchmark/mongodb_blocking_compare.php'), 'the monorepo blocking benchmark is required');
foreach (['SWOW_MONGODB_DRIVER_SO', 'SWOW_MONGODB_HOOKED_SO', 'SWOW_MONGODB_UNHOOKED_SO'] as $name) {
    skip_if(!is_file(getenv($name) ?: ''), $name . ' is required for the two-module comparison');
}
?>
--FILE--
<?php
declare(strict_types=1);

$command = [PHP_BINARY, '-n', dirname(__DIR__, 3) . '/benchmark/mongodb_blocking_compare.php',
    '--uri=' . (getenv('MONGODB_URI') ?: 'mongodb://127.0.0.1/'),
    '--mongodb-so=' . getenv('SWOW_MONGODB_DRIVER_SO'),
    '--hooked-swow-so=' . getenv('SWOW_MONGODB_HOOKED_SO'),
    '--unhooked-swow-so=' . getenv('SWOW_MONGODB_UNHOOKED_SO'),
    '--delay-ms=250', '--tick-ms=5', '--repeats=1'];
$process = proc_open($command, [0 => ['pipe', 'r'], 1 => ['pipe', 'w'], 2 => ['pipe', 'w']], $pipes);
if (!is_resource($process)) {
    throw new RuntimeException('Could not start comparison');
}
fclose($pipes[0]);
$output = stream_get_contents($pipes[1]);
$error = stream_get_contents($pipes[2]);
fclose($pipes[1]);
fclose($pipes[2]);
if (proc_close($process) !== 0) {
    throw new RuntimeException($error . $output);
}
$samples = json_decode($output, true, flags: JSON_THROW_ON_ERROR)['samples'];
var_dump(count($samples) === 2);
foreach ($samples as $sample) {
    var_dump($sample['coroutines_enabled'] && $sample['ticks_before_query'] > 0, $sample['result_valid']);
    var_dump($sample['mode'] === 'hooked' ? $sample['ticks_during_query'] > 0 : $sample['ticks_during_query'] === 0);
}
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
