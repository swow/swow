<?php

declare(strict_types=1);

/*
 * Run the test suites in order: headless, numeric, then the Tk tests when a
 * display is available. Stops at the first failing test file.
 *
 *   php tests/run.php
 */

$root = dirname(__DIR__);

$tests = [];
foreach ([
    'buffer',
    'config',
    'image',
    'render',
    'consistency',
    'window-flow',
    'worker-repair',
    'auto-reject',
    'deep',
] as $name) {
    $tests[] = [
        'name' => "headless/{$name}",
        'command' => [PHP_BINARY, '-d', 'extension=swow', $root . "/tests/headless/{$name}.php"],
    ];
}

$tests[] = [
    'name' => 'numeric',
    'command' => [PHP_BINARY, $root . '/tests/numeric/run.php'],
];

if ((getenv('DISPLAY') ?: '') !== '') {
    foreach (['layout', 'tk-smoke', 'tk-events'] as $name) {
        $tests[] = [
            'name' => "gui/{$name}",
            'command' => [PHP_BINARY, $root . "/tests/gui/{$name}.php"],
        ];
    }
}

$started = hrtime(true);
$passed = 0;
foreach ($tests as $test) {
    $process = proc_open(
        $test['command'],
        [0 => STDIN, 1 => STDOUT, 2 => STDERR],
        $pipes,
        $root,
    );
    if (!is_resource($process)) {
        printf(
            "FAIL cannot start %s, %d passed, %.2f s\n",
            $test['name'],
            $passed,
            (hrtime(true) - $started) / 1e9,
        );
        exit(1);
    }

    $status = proc_close($process);
    if ($status !== 0) {
        printf(
            "FAIL %s (exit %d), %d passed, %.2f s\n",
            $test['name'],
            $status,
            $passed,
            (hrtime(true) - $started) / 1e9,
        );
        exit(1);
    }
    $passed++;
}

printf("PASS %d test(s), %.2f s\n", $passed, (hrtime(true) - $started) / 1e9);
exit(0);
