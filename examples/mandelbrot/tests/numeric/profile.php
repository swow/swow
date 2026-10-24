<?php

declare(strict_types=1);

/**
 * Cost attribution for one frame, single thread. The loops are copies of the
 * ones in src/Renderer.php; the copies are checked against the production
 * digests on a small frame first, so the shares below describe the real code.
 *
 * Run with JIT (php -r is not optimised, a file is):
 *
 *   php -d opcache.enable_cli=1 -d opcache.jit=tracing -d opcache.jit_buffer_size=64M \
 *       tests/numeric/profile.php [width] [height] [shallow-depth] [deep-depth]
 *
 * Variants:
 *   full      palette + colour append (what production does)
 *   fixed     append a constant colour, no Palette::color call
 *   noappend  palette call, sum the colours instead of appending
 *   minimal   no palette, no append, sum the iteration counts
 */

require __DIR__ . '/bootstrap.php';
require __DIR__ . '/support.php';

use Swow\Mandelbrot\Palette;
use Swow\Mandelbrot\Reference;
use Swow\Mandelbrot\Renderer;
use Swow\Mandelbrot\View;

const FIXED_COLOR = 0x224466;
const DIVERGED = 1.0e100;
const CANCELLATION = 1.0e-12;

/** @param array<string, mixed> $params */
function directFull(array $params): array
{
    $scale = $params['scale'];
    $limit = $params['limit'];
    $width = $params['width'];
    $step = $params['step'];
    $halfWidth = $params['fullWidth'] / 2.0;
    $halfHeight = $params['fullHeight'] / 2.0;
    $centerRe = (float) $params['centerRe'];
    $centerIm = (float) $params['centerIm'];
    $colors = [];
    for ($row = 0; $row < $params['height']; $row++) {
        $ci = $centerIm + ($row * $step + $step / 2 - $halfHeight) / $scale;
        for ($column = 0; $column < $width; $column++) {
            $cr = $centerRe + ($column * $step + $step / 2 - $halfWidth) / $scale;
            $zr = 0.0;
            $zi = 0.0;
            $zr2 = 0.0;
            $zi2 = 0.0;
            $n = 0;
            while ($n < $limit && $zr2 + $zi2 <= 4.0) {
                $zi = 2.0 * $zr * $zi + $ci;
                $zr = $zr2 - $zi2 + $cr;
                $zr2 = $zr * $zr;
                $zi2 = $zi * $zi;
                $n++;
            }
            $colors[] = $n >= $limit ? Palette::INSIDE : Palette::color($n, $limit, $zr2 + $zi2);
        }
    }

    return $colors;
}

/** @param array<string, mixed> $params */
function directFixed(array $params): array
{
    $scale = $params['scale'];
    $limit = $params['limit'];
    $width = $params['width'];
    $step = $params['step'];
    $halfWidth = $params['fullWidth'] / 2.0;
    $halfHeight = $params['fullHeight'] / 2.0;
    $centerRe = (float) $params['centerRe'];
    $centerIm = (float) $params['centerIm'];
    $colors = [];
    for ($row = 0; $row < $params['height']; $row++) {
        $ci = $centerIm + ($row * $step + $step / 2 - $halfHeight) / $scale;
        for ($column = 0; $column < $width; $column++) {
            $cr = $centerRe + ($column * $step + $step / 2 - $halfWidth) / $scale;
            $zr = 0.0;
            $zi = 0.0;
            $zr2 = 0.0;
            $zi2 = 0.0;
            $n = 0;
            while ($n < $limit && $zr2 + $zi2 <= 4.0) {
                $zi = 2.0 * $zr * $zi + $ci;
                $zr = $zr2 - $zi2 + $cr;
                $zr2 = $zr * $zr;
                $zi2 = $zi * $zi;
                $n++;
            }
            $colors[] = $n >= $limit ? Palette::INSIDE : FIXED_COLOR;
        }
    }

    return $colors;
}

/** @param array<string, mixed> $params */
function directNoAppend(array $params): int
{
    $scale = $params['scale'];
    $limit = $params['limit'];
    $width = $params['width'];
    $step = $params['step'];
    $halfWidth = $params['fullWidth'] / 2.0;
    $halfHeight = $params['fullHeight'] / 2.0;
    $centerRe = (float) $params['centerRe'];
    $centerIm = (float) $params['centerIm'];
    $sum = 0;
    for ($row = 0; $row < $params['height']; $row++) {
        $ci = $centerIm + ($row * $step + $step / 2 - $halfHeight) / $scale;
        for ($column = 0; $column < $width; $column++) {
            $cr = $centerRe + ($column * $step + $step / 2 - $halfWidth) / $scale;
            $zr = 0.0;
            $zi = 0.0;
            $zr2 = 0.0;
            $zi2 = 0.0;
            $n = 0;
            while ($n < $limit && $zr2 + $zi2 <= 4.0) {
                $zi = 2.0 * $zr * $zi + $ci;
                $zr = $zr2 - $zi2 + $cr;
                $zr2 = $zr * $zr;
                $zi2 = $zi * $zi;
                $n++;
            }
            $sum += $n >= $limit ? 0 : Palette::color($n, $limit, $zr2 + $zi2);
        }
    }

    return $sum;
}

/** @param array<string, mixed> $params */
function directMinimal(array $params): int
{
    $scale = $params['scale'];
    $limit = $params['limit'];
    $width = $params['width'];
    $step = $params['step'];
    $halfWidth = $params['fullWidth'] / 2.0;
    $halfHeight = $params['fullHeight'] / 2.0;
    $centerRe = (float) $params['centerRe'];
    $centerIm = (float) $params['centerIm'];
    $sum = 0;
    for ($row = 0; $row < $params['height']; $row++) {
        $ci = $centerIm + ($row * $step + $step / 2 - $halfHeight) / $scale;
        for ($column = 0; $column < $width; $column++) {
            $cr = $centerRe + ($column * $step + $step / 2 - $halfWidth) / $scale;
            $zr = 0.0;
            $zi = 0.0;
            $zr2 = 0.0;
            $zi2 = 0.0;
            $n = 0;
            while ($n < $limit && $zr2 + $zi2 <= 4.0) {
                $zi = 2.0 * $zr * $zi + $ci;
                $zr = $zr2 - $zi2 + $cr;
                $zr2 = $zr * $zr;
                $zi2 = $zi * $zi;
                $n++;
            }
            $sum += $n;
        }
    }

    return $sum;
}

/** Shared preamble of the perturbation copies. @param array<string, mixed> $orbit */
function pertSetup(array $orbit, array $params): array
{
    $limit = $params['limit'];
    $zrList = $orbit['zr'];
    $ziList = $orbit['zi'];
    $magList = $orbit['mag'];
    $count = min(count($zrList), count($ziList), count($magList), $limit);
    $cancel = [];
    for ($n = 0; $n < $count; $n++) {
        $magnitude = $magList[$n];
        if (!is_finite($magnitude) || $magnitude >= DIVERGED) {
            $count = $n;
            break;
        }
        $cancel[$n] = $magnitude * CANCELLATION;
    }
    $lastIter = $limit - 1;
    $guard = $count < $lastIter + 1 ? $count : PHP_INT_MAX;

    return [$zrList, $ziList, $cancel, $lastIter, $guard, $count];
}

/**
 * @param array<string, mixed> $orbit
 * @param array<string, mixed> $params
 */
function pertFull(array $orbit, array $params): array
{
    $limit = $params['limit'];
    $scale = $params['scale'];
    $width = $params['width'];
    $step = $params['step'];
    $halfWidth = $params['fullWidth'] / 2.0;
    $halfHeight = $params['fullHeight'] / 2.0;
    [$zrList, $ziList, $cancel, $lastIter, $guard, $count] = pertSetup($orbit, $params);
    $colors = [];
    if ($count < 1) {
        return $colors;
    }
    for ($row = 0; $row < $params['height']; $row++) {
        $dci = ($row * $step + $step / 2 - $halfHeight) / $scale;
        for ($column = 0; $column < $width; $column++) {
            $dcr = ($column * $step + $step / 2 - $halfWidth) / $scale;
            $dxr = 0.0;
            $dxi = 0.0;
            $iter = 0;
            $index = 0;
            $zm = 0.0;
            while ($iter < $lastIter) {
                if ($index >= $guard) {
                    break;
                }
                $Zr = $zrList[$index];
                $Zi = $ziList[$index];
                $zx = $Zr + $dxr;
                $zy = $Zi + $dxi;
                $zm = $zx * $zx + $zy * $zy;
                if ($zm < $cancel[$index]) {
                    break;
                }
                if ($zm > 4.0) {
                    break;
                }
                $dm = $dxr * $dxr + $dxi * $dxi;
                if ($zm < $dm) {
                    $dxr = $zx;
                    $dxi = $zy;
                    $index = 0;
                    $Zr = $zrList[0];
                    $Zi = $ziList[0];
                }
                $pr = 2.0 * $Zr + $dxr;
                $pi = 2.0 * $Zi + $dxi;
                $ndxr = $dxr * $pr - $dxi * $pi + $dcr;
                $dxi = $dxr * $pi + $dxi * $pr + $dci;
                $dxr = $ndxr;
                $iter++;
                $index++;
            }
            if ($iter === $lastIter) {
                $Zr = $zrList[$index];
                $Zi = $ziList[$index];
                $zx = $Zr + $dxr;
                $zy = $Zi + $dxi;
                $zm = $zx * $zx + $zy * $zy;
                if (!($zm > 4.0)) {
                    $iter = $limit;
                }
            }
            $colors[] = $iter >= $limit ? Palette::INSIDE : Palette::color($iter, $limit, $zm);
        }
    }

    return $colors;
}

/**
 * @param array<string, mixed> $orbit
 * @param array<string, mixed> $params
 */
function pertFixed(array $orbit, array $params): array
{
    $limit = $params['limit'];
    $scale = $params['scale'];
    $width = $params['width'];
    $step = $params['step'];
    $halfWidth = $params['fullWidth'] / 2.0;
    $halfHeight = $params['fullHeight'] / 2.0;
    [$zrList, $ziList, $cancel, $lastIter, $guard, $count] = pertSetup($orbit, $params);
    $colors = [];
    if ($count < 1) {
        return $colors;
    }
    for ($row = 0; $row < $params['height']; $row++) {
        $dci = ($row * $step + $step / 2 - $halfHeight) / $scale;
        for ($column = 0; $column < $width; $column++) {
            $dcr = ($column * $step + $step / 2 - $halfWidth) / $scale;
            $dxr = 0.0;
            $dxi = 0.0;
            $iter = 0;
            $index = 0;
            $zm = 0.0;
            while ($iter < $lastIter) {
                if ($index >= $guard) {
                    break;
                }
                $Zr = $zrList[$index];
                $Zi = $ziList[$index];
                $zx = $Zr + $dxr;
                $zy = $Zi + $dxi;
                $zm = $zx * $zx + $zy * $zy;
                if ($zm < $cancel[$index]) {
                    break;
                }
                if ($zm > 4.0) {
                    break;
                }
                $dm = $dxr * $dxr + $dxi * $dxi;
                if ($zm < $dm) {
                    $dxr = $zx;
                    $dxi = $zy;
                    $index = 0;
                    $Zr = $zrList[0];
                    $Zi = $ziList[0];
                }
                $pr = 2.0 * $Zr + $dxr;
                $pi = 2.0 * $Zi + $dxi;
                $ndxr = $dxr * $pr - $dxi * $pi + $dcr;
                $dxi = $dxr * $pi + $dxi * $pr + $dci;
                $dxr = $ndxr;
                $iter++;
                $index++;
            }
            if ($iter === $lastIter) {
                $Zr = $zrList[$index];
                $Zi = $ziList[$index];
                $zx = $Zr + $dxr;
                $zy = $Zi + $dxi;
                $zm = $zx * $zx + $zy * $zy;
                if (!($zm > 4.0)) {
                    $iter = $limit;
                }
            }
            // same call, early return path: keeps the JIT compiling this loop
            // (without any call PHP falls back to the interpreter) and removes
            // only the palette body
            $colors[] = $iter >= $limit ? Palette::INSIDE : Palette::color($limit, $limit, 0.0);
        }
    }

    return $colors;
}

/**
 * @param array<string, mixed> $orbit
 * @param array<string, mixed> $params
 */
function pertNoAppend(array $orbit, array $params): int
{
    $limit = $params['limit'];
    $scale = $params['scale'];
    $width = $params['width'];
    $step = $params['step'];
    $halfWidth = $params['fullWidth'] / 2.0;
    $halfHeight = $params['fullHeight'] / 2.0;
    [$zrList, $ziList, $cancel, $lastIter, $guard, $count] = pertSetup($orbit, $params);
    $sum = 0;
    if ($count < 1) {
        return $sum;
    }
    for ($row = 0; $row < $params['height']; $row++) {
        $dci = ($row * $step + $step / 2 - $halfHeight) / $scale;
        for ($column = 0; $column < $width; $column++) {
            $dcr = ($column * $step + $step / 2 - $halfWidth) / $scale;
            $dxr = 0.0;
            $dxi = 0.0;
            $iter = 0;
            $index = 0;
            $zm = 0.0;
            while ($iter < $lastIter) {
                if ($index >= $guard) {
                    break;
                }
                $Zr = $zrList[$index];
                $Zi = $ziList[$index];
                $zx = $Zr + $dxr;
                $zy = $Zi + $dxi;
                $zm = $zx * $zx + $zy * $zy;
                if ($zm < $cancel[$index]) {
                    break;
                }
                if ($zm > 4.0) {
                    break;
                }
                $dm = $dxr * $dxr + $dxi * $dxi;
                if ($zm < $dm) {
                    $dxr = $zx;
                    $dxi = $zy;
                    $index = 0;
                    $Zr = $zrList[0];
                    $Zi = $ziList[0];
                }
                $pr = 2.0 * $Zr + $dxr;
                $pi = 2.0 * $Zi + $dxi;
                $ndxr = $dxr * $pr - $dxi * $pi + $dcr;
                $dxi = $dxr * $pi + $dxi * $pr + $dci;
                $dxr = $ndxr;
                $iter++;
                $index++;
            }
            if ($iter === $lastIter) {
                $Zr = $zrList[$index];
                $Zi = $ziList[$index];
                $zx = $Zr + $dxr;
                $zy = $Zi + $dxi;
                $zm = $zx * $zx + $zy * $zy;
                if (!($zm > 4.0)) {
                    $iter = $limit;
                }
            }
            $sum += $iter >= $limit ? 0 : Palette::color($iter, $limit, $zm);
        }
    }

    return $sum;
}

/**
 * @param array<string, mixed> $orbit
 * @param array<string, mixed> $params
 */
function pertMinimal(array $orbit, array $params): int
{
    $limit = $params['limit'];
    $scale = $params['scale'];
    $width = $params['width'];
    $step = $params['step'];
    $halfWidth = $params['fullWidth'] / 2.0;
    $halfHeight = $params['fullHeight'] / 2.0;
    [$zrList, $ziList, $cancel, $lastIter, $guard, $count] = pertSetup($orbit, $params);
    $sum = 0;
    if ($count < 1) {
        return $sum;
    }
    for ($row = 0; $row < $params['height']; $row++) {
        $dci = ($row * $step + $step / 2 - $halfHeight) / $scale;
        for ($column = 0; $column < $width; $column++) {
            $dcr = ($column * $step + $step / 2 - $halfWidth) / $scale;
            $dxr = 0.0;
            $dxi = 0.0;
            $iter = 0;
            $index = 0;
            $zm = 0.0;
            while ($iter < $lastIter) {
                if ($index >= $guard) {
                    break;
                }
                $Zr = $zrList[$index];
                $Zi = $ziList[$index];
                $zx = $Zr + $dxr;
                $zy = $Zi + $dxi;
                $zm = $zx * $zx + $zy * $zy;
                if ($zm < $cancel[$index]) {
                    break;
                }
                if ($zm > 4.0) {
                    break;
                }
                $dm = $dxr * $dxr + $dxi * $dxi;
                if ($zm < $dm) {
                    $dxr = $zx;
                    $dxi = $zy;
                    $index = 0;
                    $Zr = $zrList[0];
                    $Zi = $ziList[0];
                }
                $pr = 2.0 * $Zr + $dxr;
                $pi = 2.0 * $Zi + $dxi;
                $ndxr = $dxr * $pr - $dxi * $pi + $dcr;
                $dxi = $dxr * $pi + $dxi * $pr + $dci;
                $dxr = $ndxr;
                $iter++;
                $index++;
            }
            if ($iter === $lastIter) {
                $Zr = $zrList[$index];
                $Zi = $ziList[$index];
                $zx = $Zr + $dxr;
                $zy = $Zi + $dxi;
                $zm = $zx * $zx + $zy * $zy;
                if (!($zm > 4.0)) {
                    $iter = $limit;
                }
            }
            $sum += $iter;
        }
    }

    return $sum;
}


/**
 * Experiment 1: no guard compare. Only valid when the reference orbit is full
 * (guard === PHP_INT_MAX); the caller checks the digest on a full-orbit frame.
 *
 * @param array<string, mixed> $orbit
 * @param array<string, mixed> $params
 */
function pertNoGuard(array $orbit, array $params): array
{
    $limit = $params['limit'];
    $scale = $params['scale'];
    $width = $params['width'];
    $step = $params['step'];
    $halfWidth = $params['fullWidth'] / 2.0;
    $halfHeight = $params['fullHeight'] / 2.0;
    [$zrList, $ziList, $cancel, $lastIter, $guard, $count] = pertSetup($orbit, $params);
    $colors = [];
    if ($count < 1) {
        return $colors;
    }
    for ($row = 0; $row < $params['height']; $row++) {
        $dci = ($row * $step + $step / 2 - $halfHeight) / $scale;
        for ($column = 0; $column < $width; $column++) {
            $dcr = ($column * $step + $step / 2 - $halfWidth) / $scale;
            $dxr = 0.0;
            $dxi = 0.0;
            $iter = 0;
            $index = 0;
            $zm = 0.0;
            while ($iter < $lastIter) {
                $Zr = $zrList[$index];
                $Zi = $ziList[$index];
                $zx = $Zr + $dxr;
                $zy = $Zi + $dxi;
                $zm = $zx * $zx + $zy * $zy;
                if ($zm < $cancel[$index]) {
                    break;
                }
                if ($zm > 4.0) {
                    break;
                }
                $dm = $dxr * $dxr + $dxi * $dxi;
                if ($zm < $dm) {
                    $dxr = $zx;
                    $dxi = $zy;
                    $index = 0;
                    $Zr = $zrList[0];
                    $Zi = $ziList[0];
                }
                $pr = 2.0 * $Zr + $dxr;
                $pi = 2.0 * $Zi + $dxi;
                $ndxr = $dxr * $pr - $dxi * $pi + $dcr;
                $dxi = $dxr * $pi + $dxi * $pr + $dci;
                $dxr = $ndxr;
                $iter++;
                $index++;
            }
            if ($iter === $lastIter) {
                $Zr = $zrList[$index];
                $Zi = $ziList[$index];
                $zx = $Zr + $dxr;
                $zy = $Zi + $dxi;
                $zm = $zx * $zx + $zy * $zy;
                if (!($zm > 4.0)) {
                    $iter = $limit;
                }
            }
            $colors[] = $iter >= $limit ? Palette::INSIDE : Palette::color($iter, $limit, $zm);
        }
    }

    return $colors;
}

/**
 * Experiment 2: inline the rebase test, no $dm temporary.
 *
 * @param array<string, mixed> $orbit
 * @param array<string, mixed> $params
 */
function pertInlineRebase(array $orbit, array $params): array
{
    $limit = $params['limit'];
    $scale = $params['scale'];
    $width = $params['width'];
    $step = $params['step'];
    $halfWidth = $params['fullWidth'] / 2.0;
    $halfHeight = $params['fullHeight'] / 2.0;
    [$zrList, $ziList, $cancel, $lastIter, $guard, $count] = pertSetup($orbit, $params);
    $colors = [];
    if ($count < 1) {
        return $colors;
    }
    for ($row = 0; $row < $params['height']; $row++) {
        $dci = ($row * $step + $step / 2 - $halfHeight) / $scale;
        for ($column = 0; $column < $width; $column++) {
            $dcr = ($column * $step + $step / 2 - $halfWidth) / $scale;
            $dxr = 0.0;
            $dxi = 0.0;
            $iter = 0;
            $index = 0;
            $zm = 0.0;
            while ($iter < $lastIter) {
                if ($index >= $guard) {
                    break;
                }
                $Zr = $zrList[$index];
                $Zi = $ziList[$index];
                $zx = $Zr + $dxr;
                $zy = $Zi + $dxi;
                $zm = $zx * $zx + $zy * $zy;
                if ($zm < $cancel[$index]) {
                    break;
                }
                if ($zm > 4.0) {
                    break;
                }
                if ($zm < $dxr * $dxr + $dxi * $dxi) {
                    $dxr = $zx;
                    $dxi = $zy;
                    $index = 0;
                    $Zr = $zrList[0];
                    $Zi = $ziList[0];
                }
                $pr = 2.0 * $Zr + $dxr;
                $pi = 2.0 * $Zi + $dxi;
                $ndxr = $dxr * $pr - $dxi * $pi + $dcr;
                $dxi = $dxr * $pi + $dxi * $pr + $dci;
                $dxr = $ndxr;
                $iter++;
                $index++;
            }
            if ($iter === $lastIter) {
                $Zr = $zrList[$index];
                $Zi = $ziList[$index];
                $zx = $Zr + $dxr;
                $zy = $Zi + $dxi;
                $zm = $zx * $zx + $zy * $zy;
                if (!($zm > 4.0)) {
                    $iter = $limit;
                }
            }
            $colors[] = $iter >= $limit ? Palette::INSIDE : Palette::color($iter, $limit, $zm);
        }
    }

    return $colors;
}

/**
 * Experiment 3: one interleaved array with zr, zi and the cancel threshold.
 *
 * @param array<string, mixed> $orbit
 * @param array<string, mixed> $params
 */
function pertInterleaved(array $orbit, array $params): array
{
    $limit = $params['limit'];
    $scale = $params['scale'];
    $width = $params['width'];
    $step = $params['step'];
    $halfWidth = $params['fullWidth'] / 2.0;
    $halfHeight = $params['fullHeight'] / 2.0;
    [$zrList, $ziList, $cancel, $lastIter, $guard, $count] = pertSetup($orbit, $params);
    $colors = [];
    if ($count < 1) {
        return $colors;
    }
    $ref = [];
    for ($n = 0; $n < $count; $n++) {
        $ref[] = $zrList[$n];
        $ref[] = $ziList[$n];
        $ref[] = $cancel[$n];
    }
    for ($row = 0; $row < $params['height']; $row++) {
        $dci = ($row * $step + $step / 2 - $halfHeight) / $scale;
        for ($column = 0; $column < $width; $column++) {
            $dcr = ($column * $step + $step / 2 - $halfWidth) / $scale;
            $dxr = 0.0;
            $dxi = 0.0;
            $iter = 0;
            $index = 0;
            $base = 0;
            $zm = 0.0;
            while ($iter < $lastIter) {
                if ($index >= $guard) {
                    break;
                }
                $Zr = $ref[$base];
                $Zi = $ref[$base + 1];
                $zx = $Zr + $dxr;
                $zy = $Zi + $dxi;
                $zm = $zx * $zx + $zy * $zy;
                if ($zm < $ref[$base + 2]) {
                    break;
                }
                if ($zm > 4.0) {
                    break;
                }
                if ($zm < $dxr * $dxr + $dxi * $dxi) {
                    $dxr = $zx;
                    $dxi = $zy;
                    $index = 0;
                    $base = 0;
                    $Zr = $ref[0];
                    $Zi = $ref[1];
                }
                $pr = 2.0 * $Zr + $dxr;
                $pi = 2.0 * $Zi + $dxi;
                $ndxr = $dxr * $pr - $dxi * $pi + $dcr;
                $dxi = $dxr * $pi + $dxi * $pr + $dci;
                $dxr = $ndxr;
                $iter++;
                $index++;
                $base += 3;
            }
            if ($iter === $lastIter) {
                $Zr = $ref[$base];
                $Zi = $ref[$base + 1];
                $zx = $Zr + $dxr;
                $zy = $Zi + $dxi;
                $zm = $zx * $zx + $zy * $zy;
                if (!($zm > 4.0)) {
                    $iter = $limit;
                }
            }
            $colors[] = $iter >= $limit ? Palette::INSIDE : Palette::color($iter, $limit, $zm);
        }
    }

    return $colors;
}


/**
 * Experiments 1 + 2 combined: no guard compare, inline rebase test. Only valid
 * when the reference orbit is full (guard === PHP_INT_MAX).
 *
 * @param array<string, mixed> $orbit
 * @param array<string, mixed> $params
 */
function pertBoth(array $orbit, array $params): array
{
    $limit = $params['limit'];
    $scale = $params['scale'];
    $width = $params['width'];
    $step = $params['step'];
    $halfWidth = $params['fullWidth'] / 2.0;
    $halfHeight = $params['fullHeight'] / 2.0;
    [$zrList, $ziList, $cancel, $lastIter, $guard, $count] = pertSetup($orbit, $params);
    $colors = [];
    if ($count < 1) {
        return $colors;
    }
    for ($row = 0; $row < $params['height']; $row++) {
        $dci = ($row * $step + $step / 2 - $halfHeight) / $scale;
        for ($column = 0; $column < $width; $column++) {
            $dcr = ($column * $step + $step / 2 - $halfWidth) / $scale;
            $dxr = 0.0;
            $dxi = 0.0;
            $iter = 0;
            $index = 0;
            $zm = 0.0;
            while ($iter < $lastIter) {
                $Zr = $zrList[$index];
                $Zi = $ziList[$index];
                $zx = $Zr + $dxr;
                $zy = $Zi + $dxi;
                $zm = $zx * $zx + $zy * $zy;
                if ($zm < $cancel[$index]) {
                    break;
                }
                if ($zm > 4.0) {
                    break;
                }
                if ($zm < $dxr * $dxr + $dxi * $dxi) {
                    $dxr = $zx;
                    $dxi = $zy;
                    $index = 0;
                    $Zr = $zrList[0];
                    $Zi = $ziList[0];
                }
                $pr = 2.0 * $Zr + $dxr;
                $pi = 2.0 * $Zi + $dxi;
                $ndxr = $dxr * $pr - $dxi * $pi + $dcr;
                $dxi = $dxr * $pi + $dxi * $pr + $dci;
                $dxr = $ndxr;
                $iter++;
                $index++;
            }
            if ($iter === $lastIter) {
                $Zr = $zrList[$index];
                $Zi = $ziList[$index];
                $zx = $Zr + $dxr;
                $zy = $Zi + $dxi;
                $zm = $zx * $zx + $zy * $zy;
                if (!($zm > 4.0)) {
                    $iter = $limit;
                }
            }
            $colors[] = $iter >= $limit ? Palette::INSIDE : Palette::color($iter, $limit, $zm);
        }
    }

    return $colors;
}


/**
 * Experiment 4: two pixels per reference step. The shared reference read is
 * only valid while both pixels are on the same index; a rebase of either pixel
 * rebases both (mathematically exact) and keeps them synced. When either
 * escapes or glitches the rest of that pixel runs through finishPixel().
 *
 * @param array<string, mixed> $orbit
 * @param array<string, mixed> $params
 */
function pertPair(array $orbit, array $params): array
{
    $limit = $params['limit'];
    $scale = $params['scale'];
    $width = $params['width'];
    $step = $params['step'];
    $halfWidth = $params['fullWidth'] / 2.0;
    $halfHeight = $params['fullHeight'] / 2.0;
    [$zrList, $ziList, $cancel, $lastIter, $guard, $count] = pertSetup($orbit, $params);
    $colors = [];
    if ($count < 1) {
        return $colors;
    }
    for ($row = 0; $row < $params['height']; $row++) {
        $dci = ($row * $step + $step / 2 - $halfHeight) / $scale;
        $column = 0;
        while ($column + 1 < $width) {
            $dcrA = ($column * $step + $step / 2 - $halfWidth) / $scale;
            $dcrB = (($column + 1) * $step + $step / 2 - $halfWidth) / $scale;
            $dxrA = 0.0;
            $dxiA = 0.0;
            $dxrB = 0.0;
            $dxiB = 0.0;
            $iter = 0;
            $index = 0;
            $zmA = 0.0;
            $zmB = 0.0;
            $stateA = null;
            $stateB = null;
            while ($iter < $lastIter) {
                if ($index >= $guard) {
                    $stateA = [$iter, $index, $dxrA, $dxiA, true];
                    $stateB = [$iter, $index, $dxrB, $dxiB, true];
                    break;
                }
                $Zr = $zrList[$index];
                $Zi = $ziList[$index];
                $threshold = $cancel[$index];
                $zxA = $Zr + $dxrA;
                $zyA = $Zi + $dxiA;
                $zmA = $zxA * $zxA + $zyA * $zyA;
                $zxB = $Zr + $dxrB;
                $zyB = $Zi + $dxiB;
                $zmB = $zxB * $zxB + $zyB * $zyB;
                if ($zmA < $threshold || $zmB < $threshold) {
                    $stateA = [$iter, $index, $dxrA, $dxiA, true];
                    $stateB = [$iter, $index, $dxrB, $dxiB, true];
                    break;
                }
                if ($zmA > 4.0) {
                    $stateA = [$iter, $index, $dxrA, $dxiA, false];
                    $stateB = null;
                    $zmB = $zmB;
                    break;
                }
                if ($zmB > 4.0) {
                    $stateA = null;
                    $stateB = [$iter, $index, $dxrB, $dxiB, false];
                    break;
                }
                if ($zmA < $dxrA * $dxrA + $dxiA * $dxiA || $zmB < $dxrB * $dxrB + $dxiB * $dxiB) {
                    $dxrA = $zxA;
                    $dxiA = $zyA;
                    $dxrB = $zxB;
                    $dxiB = $zyB;
                    $index = 0;
                    $Zr = $zrList[0];
                    $Zi = $ziList[0];
                }
                $prA = 2.0 * $Zr + $dxrA;
                $piA = 2.0 * $Zi + $dxiA;
                $prB = 2.0 * $Zr + $dxrB;
                $piB = 2.0 * $Zi + $dxiB;
                $ndxrA = $dxrA * $prA - $dxiA * $piA + $dcrA;
                $ndxiA = $dxrA * $piA + $dxiA * $prA + $dci;
                $ndxrB = $dxrB * $prB - $dxiB * $piB + $dcrB;
                $ndxiB = $dxrB * $piB + $dxiB * $prB + $dci;
                $dxrA = $ndxrA;
                $dxiA = $ndxiA;
                $dxrB = $ndxrB;
                $dxiB = $ndxiB;
                $iter++;
                $index++;
            }
            $resultA = $stateA === null
                ? finishPixel($zrList, $ziList, $cancel, $guard, $lastIter, $limit, $dcrA, $dci, $iter, $index, $dxrA, $dxiA, false)
                : finishPixel($zrList, $ziList, $cancel, $guard, $lastIter, $limit, $dcrA, $dci, $stateA[0], $stateA[1], $stateA[2], $stateA[3], $stateA[4]);
            $resultB = $stateB === null
                ? finishPixel($zrList, $ziList, $cancel, $guard, $lastIter, $limit, $dcrB, $dci, $iter, $index, $dxrB, $dxiB, false)
                : finishPixel($zrList, $ziList, $cancel, $guard, $lastIter, $limit, $dcrB, $dci, $stateB[0], $stateB[1], $stateB[2], $stateB[3], $stateB[4]);
            $colors[] = $resultA[2] ? Palette::INSIDE : Palette::color($resultA[0], $limit, $resultA[1]);
            $colors[] = $resultB[2] ? Palette::INSIDE : Palette::color($resultB[0], $limit, $resultB[1]);
            $column += 2;
        }
        // odd last column
        if ($column < $width) {
            $dcr = ($column * $step + $step / 2 - $halfWidth) / $scale;
            $single = finishPixel($zrList, $ziList, $cancel, $guard, $lastIter, $limit, $dcr, $dci, 0, 0, 0.0, 0.0, false);
            $colors[] = $single[2] ? Palette::INSIDE : Palette::color($single[0], $limit, $single[1]);
        }
    }

    return $colors;
}

/**
 * Single pixel loop from a given state, used to finish a pixel the pair loop
 * could not keep. Returns [iteration, |z|^2, glitch].
 *
 * @param list<float> $zrList
 * @param list<float> $ziList
 * @param list<float> $cancel
 * @return array{0: int, 1: float, 2: bool}
 */
function finishPixel(array $zrList, array $ziList, array $cancel, int $guard, int $lastIter, int $limit,
    float $dcr, float $dci, int $iter, int $index, float $dxr, float $dxi, bool $glitch): array
{
    $zm = 0.0;
    while ($iter < $lastIter) {
        if ($index >= $guard) {
            $glitch = true;
            break;
        }
        $Zr = $zrList[$index];
        $Zi = $ziList[$index];
        $zx = $Zr + $dxr;
        $zy = $Zi + $dxi;
        $zm = $zx * $zx + $zy * $zy;
        if ($zm < $cancel[$index]) {
            $glitch = true;
            break;
        }
        if ($zm > 4.0) {
            break;
        }
        if ($zm < $dxr * $dxr + $dxi * $dxi) {
            $dxr = $zx;
            $dxi = $zy;
            $index = 0;
            $Zr = $zrList[0];
            $Zi = $ziList[0];
        }
        $pr = 2.0 * $Zr + $dxr;
        $pi = 2.0 * $Zi + $dxi;
        $ndxr = $dxr * $pr - $dxi * $pi + $dcr;
        $dxi = $dxr * $pi + $dxi * $pr + $dci;
        $dxr = $ndxr;
        $iter++;
        $index++;
    }
    if ($iter === $lastIter) {
        $Zr = $zrList[$index];
        $Zi = $ziList[$index];
        $zx = $Zr + $dxr;
        $zy = $Zi + $dxi;
        $zm = $zx * $zx + $zy * $zy;
        if (!($zm > 4.0)) {
            $iter = $limit;
            if (!($zm <= 4.0)) {
                $glitch = true;
            }
        }
    }

    return [$iter, $zm, $glitch || $iter >= $limit];
}

/**
 * One variant over the prepared data. Kept apart so that each variant can run
 * in its own process: in a single process the JIT does not compile every hot
 * loop, and the interpreter fallback hides the real shares.
 *
 * @param array<string, mixed> $params
 * @param array<string, mixed> $orbit
 */
function runVariant(string $name, array $params, array $orbit, int $height): mixed
{
    return match ($name) {
        'direct-production' => Renderer::renderBand($orbit, $params + ['path' => 'direct'], 0, $height),
        'direct-full' => directFull($params),
        'direct-fixed' => directFixed($params),
        'direct-noappend' => directNoAppend($params),
        'direct-minimal' => directMinimal($params),
        'pert-full' => pertFull($orbit, $params),
        'pert-noguard' => pertNoGuard($orbit, $params),
        'pert-both' => pertBoth($orbit, $params),
        'pert-pair' => pertPair($orbit, $params),
        'pert-inline' => pertInlineRebase($orbit, $params),
        'pert-interleaved' => pertInterleaved($orbit, $params),
        'pert-fixed' => pertFixed($orbit, $params),
        'pert-noappend' => pertNoAppend($orbit, $params),
        'pert-minimal' => pertMinimal($orbit, $params),
        default => throw new InvalidArgumentException("unknown variant {$name}"),
    };
}

/** @return array{0: array<string, mixed>, 1: array<string, mixed>} params and orbit */
function prepare(int $width, int $height, string $depth, string $centre = 'default'): array
{
    $view = new View($width, $height);
    if ($centre === 'deep') {
        $view->setCenter(DEEP_CENTER_RE, DEEP_CENTER_IM);
    }
    $view->setDepth($depth);
    $reference = new Reference($view);
    $reference->build();

    return [$view->params($width, $height, 1), $reference->orbit()];
}

$width = (int) ($argv[1] ?? 1280);
$height = (int) ($argv[2] ?? 676);
$shallowDepth = $argv[3] ?? '1e4';
$deepDepth = $argv[4] ?? '1e10';
$only = $argv[5] ?? null;
$centre = $argv[6] ?? 'default';

$variantNames = [
    'direct-production', 'direct-full', 'direct-fixed', 'direct-noappend', 'direct-minimal',
    'pert-full', 'pert-fixed', 'pert-noappend', 'pert-minimal',
    'pert-noguard', 'pert-inline', 'pert-interleaved', 'pert-both', 'pert-pair',
];
$filter = null;
if ($only !== null && !in_array($only, $variantNames, true)) {
    $filter = $only === 'direct' || $only === 'pert' ? $only : null;
    $only = null;
}

if ($only !== null) {
    // child mode: time one variant in a clean process and print one line
    $frameCentre = str_starts_with($only, 'direct') ? 'default' : $centre;
    [$params, $orbit] = prepare($width, $height, str_starts_with($only, 'direct') ? $shallowDepth : $deepDepth, $frameCentre);
    $runs = str_starts_with($only, 'direct') || str_starts_with($only, 'pert') ? 2 : 1;
    $best = INF;
    for ($i = 0; $i < $runs; $i++) {
        $start = hrtime(true);
        runVariant($only, $params, $orbit, $height);
        $seconds = (hrtime(true) - $start) / 1e9;
        if ($seconds < $best) {
            $best = $seconds;
        }
    }
    printf("%s %.4f\n", $only, $best);
    if (getenv('PROFILE_JIT_STATUS')) {
        $status = opcache_get_status(false)['jit'] ?? [];
        fwrite(STDERR, sprintf("jit status: %s\n", json_encode($status)));
    }
    exit(0);
}

// fidelity: the copies must reproduce the production image on a small frame
$check = new View(128, 72);
$check->setDepth($shallowDepth);
$checkReference = new Reference($check);
$checkReference->build();
$checkParams = $check->params(128, 72, 1);
$checkOrbit = $checkReference->orbit();
checkSame(
    digestColors(runVariant('direct-production', $checkParams, $checkOrbit, 72)['colors']),
    digestColors(directFull($checkParams)),
    'direct copy matches production'
);
$check->setDepth($deepDepth);
if ($centre === 'deep') {
    $check->setCenter(DEEP_CENTER_RE, DEEP_CENTER_IM);
}
$checkReference->build();
$checkParams = $check->params(128, 72, 1);
$checkOrbit = $checkReference->orbit();
checkSame(
    digestColors(runVariant('pert-full', $checkParams, $checkOrbit, 72)),
    digestColors(Renderer::renderBand($checkOrbit, $checkParams + ['path' => 'perturb'], 0, 72)['colors']),
    'perturbation copy matches production'
);
$experimentDigests = [];
foreach (['pert-noguard', 'pert-inline', 'pert-interleaved', 'pert-both', 'pert-pair'] as $name) {
    $experimentDigests[$name] = digestColors(runVariant($name, $checkParams, $checkOrbit, 72));
}
$referenceDigest = digestColors(runVariant('pert-full', $checkParams, $checkOrbit, 72));
foreach ($experimentDigests as $name => $digest) {
    checkSame($referenceDigest, $digest, "{$name} digest unchanged");
}
unset($checkReference);

$jitFlags = ['-d', 'opcache.enable_cli=1', '-d', 'opcache.jit=tracing', '-d', 'opcache.jit_buffer_size=64M'];
$children = [
    'direct-production' => 'production renderBand path=direct',
    'direct-full' => 'full (palette + append)',
    'direct-fixed' => 'fixed colour (append only)',
    'direct-noappend' => 'no append (palette + sum)',
    'direct-minimal' => 'minimal (loop + sum)',
    'pert-full' => 'full (palette + append)',
    'pert-noguard' => 'exp 1: no guard compare',
    'pert-inline' => 'exp 2: inline rebase test',
    'pert-interleaved' => 'exp 3: interleaved ref array',
    'pert-noappend' => 'no append (palette + sum)',
    'pert-pair' => 'exp 4: two pixels per step',
    'pert-both' => 'exp 1+2: both',
];
$seconds = [];
foreach ($children as $name => $label) {
    if ($filter !== null && !str_starts_with($name, $filter . '-')) {
        continue;
    }
    $command = escapeshellarg(PHP_BINARY);
    foreach ($jitFlags as $flag) {
        $command .= ' ' . escapeshellarg($flag);
    }
    $command .= ' ' . escapeshellarg(__FILE__) . ' ' . $width . ' ' . $height . ' '
        . escapeshellarg($shallowDepth) . ' ' . escapeshellarg($deepDepth) . ' ' . escapeshellarg($name)
        . ' ' . escapeshellarg($centre);
    $output = [];
    exec($command, $output, $code);
    if ($code !== 0 || count($output) === 0) {
        fwrite(STDERR, "child failed: {$name}\n");
        exit(1);
    }
    $seconds[$name] = (float) explode(' ', trim(end($output)))[1];
}

function share(float $part, float $full): string
{
    return sprintf('%6.1f%%', 100.0 * $part / $full);
}

foreach ([['direct', $shallowDepth], ['pert', $deepDepth]] as [$prefix, $depth]) {
    if ($filter !== null && $filter !== $prefix) {
        continue;
    }
    $full = $seconds["{$prefix}-full"] ?? NAN;
    $fixed = $seconds["{$prefix}-fixed"] ?? null;
    $noAppend = $seconds["{$prefix}-noappend"] ?? null;
    $minimal = $seconds["{$prefix}-minimal"] ?? null;
    printf("\n%s path, %dx%d, depth %s, single thread, JIT, each row a fresh process\n",
        $prefix === 'direct' ? 'Direct' : 'Perturbation', $width, $height, $depth);
    printf("%-38s %9s %9s\n", 'variant', 'seconds', 'of full');
    if (isset($seconds["{$prefix}-production"])) {
        printf("%-38s %9.4f %9s\n", 'production renderBand', $seconds["{$prefix}-production"], '-');
    }
    printf("%-38s %9.4f %9s\n", 'full (palette + append)', $full, '100.0%');
    if ($fixed !== null) {
        printf("%-38s %9.4f %9s\n", 'fixed colour (append only)', $fixed, share($fixed, $full));
    }
    if ($noAppend !== null) {
        printf("%-38s %9.4f %9s\n", 'no append (palette + sum)', $noAppend, share($noAppend, $full));
    }
    if ($minimal !== null) {
        printf("%-38s %9.4f %9s\n", 'minimal (loop + sum)', $minimal, share($minimal, $full));
    }
    foreach (['pert-noguard' => 'exp 1: no guard compare', 'pert-inline' => 'exp 2: inline rebase test',
              'pert-interleaved' => 'exp 3: interleaved ref array', 'pert-both' => 'exp 1+2: both',
              'pert-pair' => 'exp 4: two pixels per step'] as $name => $label) {
        if (!isset($seconds[$name])) {
            continue;
        }
        printf("%-38s %9.4f %9s\n", $label, $seconds[$name], share($seconds[$name], $full));
    }
    if ($fixed !== null && $noAppend !== null && $minimal !== null) {
        printf("  palette %.4f s %s | append %.4f s %s | core %.4f s %s\n",
            $full - $fixed, share($full - $fixed, $full),
            $full - $noAppend, share($full - $noAppend, $full),
            $minimal, share($minimal, $full));
    }
}

printf("\n%d failure(s)\n", $GLOBALS['failures'] ?? 0);
