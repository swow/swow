<?php

declare(strict_types=1);

/**
 * Numeric core tests. Plain PHP, bcmath only, no swow.
 *
 *   php tests/numeric/run.php            run everything
 *   php tests/numeric/run.php view       run one group
 */

require __DIR__ . '/bootstrap.php';
require __DIR__ . '/support.php';
require __DIR__ . '/test_view.php';
require __DIR__ . '/test_reference.php';
require __DIR__ . '/test_renderer.php';
require __DIR__ . '/test_perturbation.php';
require __DIR__ . '/test_glitch.php';
require __DIR__ . '/test_fixtures.php';
require __DIR__ . '/test_roundtrip.php';

$GLOBALS['failures'] = 0;
$groups = ['view', 'reference', 'renderer', 'glitch', 'perturbation', 'fixtures', 'roundtrip'];
$only = $argv[1] ?? 'all';
if ($only !== 'all' && !in_array($only, $groups, true)) {
    fwrite(STDERR, "unknown group '{$only}', use one of: all " . implode(' ', $groups) . "\n");
    exit(2);
}

$started = hrtime(true);
foreach ($groups as $group) {
    if ($only !== 'all' && $only !== $group) {
        continue;
    }
    $function = 'test' . ucfirst($group);
    $begin = hrtime(true);
    $function();
    printf("  -- %s in %.2f s\n", $group, (hrtime(true) - $begin) / 1e9);
}

$failures = $GLOBALS['failures'];
printf("\n%s, %d failure(s), %.2f s\n", $failures === 0 ? 'PASS' : 'FAIL', $failures, (hrtime(true) - $started) / 1e9);
exit($failures === 0 ? 0 : 1);
