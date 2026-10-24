<?php

declare(strict_types=1);

/*
 * Shared helpers for the headless tests.
 *
 * A test file requires this file, calls check() for each expectation, and
 * calls finish() with its suite name at the end. Tests stay runnable on their
 * own.
 */

function fail(string $what): never
{
    fwrite(STDERR, "FAIL: {$what}\n");
    exit(1);
}

function check(bool $condition, string $what): void
{
    if (!$condition) {
        fail($what);
    }
}

/** Print the suite result line and exit. */
function finish(string $name): never
{
    echo "{$name}: ok\n";
    exit(0);
}

/** Path in the system temp directory, unique to this process. */
function tempPath(string $name, string $extension = ''): string
{
    return sys_get_temp_dir() . '/mandelbrot-' . $name . '-' . getmypid() . $extension;
}

/** Remove files a test created; missing paths are ignored. */
function removeTempFiles(string ...$paths): void
{
    foreach ($paths as $path) {
        if (is_file($path)) {
            unlink($path);
        }
    }
}

/** Stable 64 bit digest of a colour list, independent of the PHP int size. */
function digestColors(array $colors): string
{
    $bytes = '';
    foreach ($colors as $color) {
        $bytes .= chr(($color >> 16) & 0xFF) . chr(($color >> 8) & 0xFF) . chr($color & 0xFF);
    }

    return hash('xxh3', $bytes);
}

/*
 * The deep fixture from tests/numeric/support.php. The headless suite keeps
 * its own copy so it does not load the numeric helpers.
 */
const DEEP_CENTER_RE = '-0.743643887018689113498922249712800000007037228233600378464531';
const DEEP_CENTER_IM = '0.131825904211894657975878336475200000004669779776843260798656';
