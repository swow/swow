<?php

declare(strict_types=1);

require dirname(__DIR__) . '/autoload.php';

use Swow\Examples\Gemm\Bench\Runner;
use Swow\Examples\Gemm\Format\F32Format;
use Swow\Examples\Gemm\Format\Q4_0Format;
use Swow\Examples\Gemm\Format\Q8_0Format;
use Swow\Examples\Gemm\Support\CpuAffinity;

function usage(): void
{
    fwrite(STDERR, <<<'TXT'
usage: gemv.php <f32|q4_0|q8_0> [k] [n] [threads] [options]
       gemv.php <format> both [threads] [options]

  k, n        default 1024 and 4096; both runs 1024x4096 and 1536x6144
  threads     default physical cores
  --pin=auto|sysfs|compact   pin workers to one CPU per core
  --warmup=N  default 3
  --iterations=N default 20

TXT);
    exit(1);
}

$arguments = array_slice($argv, 1);
$name = array_shift($arguments);
$formats = ['f32' => F32Format::class, 'q4_0' => Q4_0Format::class, 'q8_0' => Q8_0Format::class];
if ($name === null || !isset($formats[$name])) {
    usage();
}
$format = new $formats[$name]();

$pin = null;
$warmup = 3;
$iterations = 20;
$positional = [];
foreach ($arguments as $argument) {
    if (str_starts_with($argument, '--pin=')) {
        $pin = substr($argument, 6);
        continue;
    }
    if (str_starts_with($argument, '--warmup=')) {
        $warmup = max(0, (int) substr($argument, 9));
        continue;
    }
    if (str_starts_with($argument, '--iterations=')) {
        $iterations = max(1, (int) substr($argument, 13));
        continue;
    }
    if (str_starts_with($argument, '--')) {
        usage();
    }
    $positional[] = $argument;
}

$threads = max(1, count(CpuAffinity::physicalCpus('auto')));

try {
    if (($positional[0] ?? '') === 'both') {
        if (count($positional) > 2) {
            usage();
        }
        $threads = max(1, (int) ($positional[1] ?? $threads));
        Runner::benchmark($format, 1024, 4096, $threads, $pin, $warmup, $iterations);
        Runner::benchmark($format, 1536, 6144, $threads, $pin, $warmup, $iterations);
    } else {
        if (count($positional) > 3) {
            usage();
        }
        $k = (int) ($positional[0] ?? 1024);
        $n = (int) ($positional[1] ?? 4096);
        $threads = max(1, (int) ($positional[2] ?? $threads));
        Runner::benchmark($format, $k, $n, $threads, $pin, $warmup, $iterations);
    }
} catch (Throwable $e) {
    fwrite(STDERR, $e::class . ': ' . $e->getMessage() . "\n");
    exit(1);
}
