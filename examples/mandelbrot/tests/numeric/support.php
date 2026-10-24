<?php

declare(strict_types=1);

/**
 * Shared test helpers. The bcmath and double iteration here are written
 * independently of src/ so the tests do not compare a function with itself.
 */

use Swow\Mandelbrot\Palette;
use Swow\Mandelbrot\View;

/** Print a line and remember a failure. */
function check(bool $ok, string $what): void
{
    if ($ok) {
        echo "  ok   {$what}\n";
        return;
    }
    echo "  FAIL {$what}\n";
    $GLOBALS['failures'] = ($GLOBALS['failures'] ?? 0) + 1;
}

function checkSame(mixed $expected, mixed $actual, string $what): void
{
    if ($expected === $actual) {
        echo "  ok   {$what}\n";
        return;
    }
    echo "  FAIL {$what}: expected " . var_export($expected, true) . ', got ' . var_export($actual, true) . "\n";
    $GLOBALS['failures'] = ($GLOBALS['failures'] ?? 0) + 1;
}

function checkNear(float $expected, float $actual, float $tolerance, string $what): void
{
    $ok = abs($expected - $actual) <= $tolerance;
    echo ($ok ? '  ok   ' : '  FAIL ') . $what . ($ok ? '' : sprintf(' (expected %.17g, got %.17g)', $expected, $actual)) . "\n";
    if (!$ok) {
        $GLOBALS['failures'] = ($GLOBALS['failures'] ?? 0) + 1;
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

/**
 * Exact pixel centre as a decimal string, computed the same way View::params()
 * defines the offset. Returns [re, im].
 */
function pixelCenter(View $view, int $x, int $y, int $step, int $precision, float $scale): array
{
    $scaleText = $scale >= 1.0 ? number_format($scale, 0, '.', '') : number_format($scale, 20, '.', '');
    $numeratorX = $x * $step + $step / 2 - $view->width() / 2;
    // row 0 is the top of the image and the highest imaginary part
    $numeratorY = $view->height() / 2 - ($y * $step + $step / 2);

    return [
        bcadd($view->centerRe(), bcdiv(sprintf('%.1f', $numeratorX), $scaleText, $precision), $precision),
        bcadd($view->centerIm(), bcdiv(sprintf('%.1f', $numeratorY), $scaleText, $precision), $precision),
    ];
}

/**
 * Direct bcmath iteration of the orbit of c.
 *
 * @return array{0: int, 1: float} escape count and |z|^2, count equals the limit
 *                                 when the pixel did not escape
 */
function bcmathPixel(string $cRe, string $cIm, int $precision, int $limit): array
{
    $zr = '0';
    $zi = '0';
    for ($n = 0; $n < $limit; $n++) {
        $zr2 = bcmul($zr, $zr, $precision);
        $zi2 = bcmul($zi, $zi, $precision);
        $magnitude = bcadd($zr2, $zi2, $precision);
        if (bccomp($magnitude, '4', $precision) > 0) {
            return [$n, (float) $magnitude];
        }
        $nextZi = bcadd(bcmul(bcadd($zr, $zr, $precision), $zi, $precision), $cIm, $precision);
        $zr = bcadd(bcsub($zr2, $zi2, $precision), $cRe, $precision);
        $zi = $nextZi;
    }

    return [$limit, 0.0];
}

/**
 * Direct double iteration of c, the reference the perturbation path must agree
 * with wherever double precision can decide the pixel.
 *
 * @return array{0: int, 1: float}
 */
function doublePixel(float $cRe, float $cIm, int $limit): array
{
    $zr = 0.0;
    $zi = 0.0;
    $zr2 = 0.0;
    $zi2 = 0.0;
    $n = 0;
    while ($n < $limit && $zr2 + $zi2 <= 4.0) {
        $zi = 2.0 * $zr * $zi + $cIm;
        $zr = $zr2 - $zi2 + $cRe;
        $zr2 = $zr * $zr;
        $zi2 = $zi * $zi;
        $n++;
    }

    return [$n, $zr2 + $zi2];
}

/** Colour of [count, magnitude] as the production palette makes it. */
function pixelColor(int $count, float $magnitudeSquared, int $limit): int
{
    return $count >= $limit ? Palette::INSIDE : Palette::color($count, $limit, $magnitudeSquared);
}

/** Apply a flat glitch list with Reference::renderPixelSlow(). */
function repairGlitches(array $colors, array $glitches, Swow\Mandelbrot\Reference $reference, int $width, int $limit, int $step = 1): array
{
    for ($i = 0; $i < count($glitches); $i += 2) {
        $x = $glitches[$i];
        $y = $glitches[$i + 1];
        $colors[$y * $width + $x] = $reference->renderPixelSlow($x, $y, $step, $limit);
    }

    return $colors;
}

/**
 * A boundary-adjacent coordinate found by iterative refinement from the default
 * view. The default centre leaves the set and its nearest structure is about
 * 1e-15 away, so it renders as one flat colour past depth 1e15. This point keeps
 * escape-time structure up to depth 1e30 (hundreds of colours, glitch free).
 */
const DEEP_CENTER_RE = '-0.743643887018689113498922249712800000007037228233600378464531';
const DEEP_CENTER_IM = '0.131825904211894657975878336475200000004669779776843260798656';
