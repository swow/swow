<?php

declare(strict_types=1);

/*
 * --auto is a window-mode feature; with --headless the CLI must fail before
 * rendering a frame or starting any thread.
 */

require __DIR__ . '/../support.php';

$command = escapeshellarg(PHP_BINARY)
    . ' -d extension=swow '
    . escapeshellarg(__DIR__ . '/../../bin/mandelbrot')
    . ' --headless --auto=50 2>&1';
exec($command, $output, $status);
$text = implode("\n", $output);

check($status === 2, 'headless auto exits with the usage error status');
check(str_contains($text, 'mandelbrot: --auto needs a window'), 'headless auto stderr line');
check(preg_match('/^frame /m', $text) === 0, 'headless auto does not start a frame');

finish('auto-reject');
