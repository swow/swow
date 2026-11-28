--TEST--
swow_debug: sampling profiler folds identical stacks
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
skip_if_in_valgrind();
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use function Swow\Debug\getProfile;
use function Swow\Debug\startProfiler;
use function Swow\Debug\stopProfiler;

function profiler_hot_loop(float $seconds): void
{
    $start = microtime(true);
    while (microtime(true) - $start < $seconds) {
        $value = md5('swow');
    }
}

Assert::same(getProfile()['samples'], 0);
startProfiler(1);
Assert::throws(static fn() => startProfiler(1), Error::class);
profiler_hot_loop(0.2);
stopProfiler();
Assert::throws(static fn() => stopProfiler(), Error::class);

$profile = getProfile();
Assert::false($profile['running']);
Assert::greaterThan($profile['samples'], 10);
$hot = 0;
foreach ($profile['stacks'] as $stack) {
    foreach ($stack['frames'] as $frame) {
        if ($frame['function'] === 'profiler_hot_loop') {
            $hot += $stack['count'];
            break;
        }
    }
}
Assert::greaterThan($hot, ($profile['samples'] - $profile['idleSamples']) / 2);
Assert::greaterThanEq($profile['stacks'][0]['count'], $profile['stacks'][count($profile['stacks']) - 1]['count']);

getProfile(true);
Assert::same(getProfile()['samples'], 0);

echo "Done\n";
?>
--EXPECT--
Done
