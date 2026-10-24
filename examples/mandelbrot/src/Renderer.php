<?php

declare(strict_types=1);

namespace Swow\Mandelbrot;

use function count;

use const PHP_INT_MAX;

/**
 * Renders bands of pixels, either by iterating c in double directly or by the
 * perturbation method.
 *
 * Row 0 is the top row of the image and has the highest imaginary part. Shallow
 * views take the direct double loop; deeper views follow a reference orbit. The
 * perturbation step is dz = dz * (2*Z + dz) + dc and it rebases onto reference
 * index 0 when |Z + dz|^2 < |dz|^2. The rebase is exact because Z(0) = 0 and
 * the orbit is deterministic.
 *
 * A pixel that passes the reference escape keeps its value Z + dz and finishes
 * with the direct double iteration. A pixel that loses precision against the
 * reference is reported in the glitch list, and the main thread re-renders it
 * with Reference::renderPixelSlow().
 */
final class Renderer
{
    /** A reference orbit past this |Z|^2 has escaped and is not usable. */
    private const DIVERGED = 1.0e100;

    /**
     * Views below this depth are drawn by the direct double iteration. The
     * perturbation loop costs about three times as much per step, and the
     * direct c = centre + offset is accurate to a small fraction of a pixel up
     * to 1e10, so switching earlier only costs time.
     */
    public const DIRECT_DEPTH = 1.0e10;

    /**
     * Pauldelbrot's glitch test: |Z + dz|^2 below |Z|^2 times this means the
     * pixel value carries no relative precision against the reference and the
     * slow path has to draw it. 1e-12 is a |z| below 1e-6 |Z|.
     */
    public const CANCELLATION = 1.0e-12;

    /**
     * @param array{zr: list<float>, zi: list<float>, mag: list<float>} $orbit
     * @param array{centerRe: string, centerIm: string, scale: float, limit: int,
     *     fullWidth: int, fullHeight: int, width: int, height: int, step: int} $params
     * @return array{colors: list<int>, glitches: list<int>}
     */
    public static function renderBand(array $orbit, array $params, int $rowBegin, int $rowEnd): array
    {
        if ($rowEnd <= $rowBegin || $params['width'] < 1 || $params['height'] < 1) {
            return ['colors' => [], 'glitches' => []];
        }

        // params may force one path for verification and benchmarks
        $path = $params['path'] ?? 'auto';
        if ($path === 'perturb') {
            return self::renderBandPerturbation($orbit, $params, $rowBegin, $rowEnd);
        }
        if ($path !== 'direct') {
            $unit = self::BASE_SCALE * $params['fullHeight'] / 1080.0;
            if ($unit <= 0.0 || $params['scale'] < $unit * self::DIRECT_DEPTH) {
                $path = 'direct';
            }
        }
        if ($path === 'direct') {
            return self::renderBandDirect($params, $rowBegin, $rowEnd);
        }

        return self::renderBandPerturbation($orbit, $params, $rowBegin, $rowEnd);
    }

    /** Pixels per unit of the reset view at a height of 1080. */
    private const BASE_SCALE = 300.0;

    /**
     * @param array{centerRe: string, centerIm: string, scale: float, limit: int,
     *     fullWidth: int, fullHeight: int, width: int, height: int, step: int} $params
     * @return array{colors: list<int>, glitches: list<int>}
     */
    private static function renderBandDirect(array $params, int $rowBegin, int $rowEnd): array
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
        for ($row = $rowBegin; $row < $rowEnd; $row++) {
            // row 0 is the top of the image and the highest imaginary part
            $ci = $centerIm - ($row * $step + $step / 2 - $halfHeight) / $scale;
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

        return ['colors' => $colors, 'glitches' => []];
    }

    /**
     * @param array{zr: list<float>, zi: list<float>, mag: list<float>} $orbit
     * @param array{centerRe: string, centerIm: string, scale: float, limit: int,
     *     fullWidth: int, fullHeight: int, width: int, height: int, step: int} $params
     * @return array{colors: list<int>, glitches: list<int>}
     */
    private static function renderBandPerturbation(array $orbit, array $params, int $rowBegin, int $rowEnd): array
    {
        $limit = $params['limit'];
        $scale = $params['scale'];
        $width = $params['width'];
        $step = $params['step'];
        $halfWidth = $params['fullWidth'] / 2.0;
        $halfHeight = $params['fullHeight'] / 2.0;

        $zrList = $orbit['zr'];
        $ziList = $orbit['zi'];
        $magList = $orbit['mag'];
        $centreRe = (float) $params['centerRe'];
        $centreIm = (float) $params['centerIm'];
        $count = min(count($zrList), count($ziList), count($magList), $limit);
        // squared |Z| below which a pixel is treated as a glitch, and the index
        // where the reference orbit leaves the escape disc for the last time
        $cancel = [];
        $escapeIndex = PHP_INT_MAX;
        for ($n = 0; $n < $count; $n++) {
            $magnitude = $magList[$n];
            if (!is_finite($magnitude) || $magnitude >= self::DIVERGED) {
                $count = $n;
                break;
            }
            if ($escapeIndex === PHP_INT_MAX && $magnitude > 4.0) {
                $escapeIndex = $n;
            }
            $cancel[$n] = $magnitude * self::CANCELLATION;
        }
        $colors = [];
        $glitches = [];
        if ($count < 1) {
            // Without a reference orbit the perturbation has nothing to walk.
            // Every pixel is reported so the slow path can draw the band.
            for ($row = $rowBegin; $row < $rowEnd; $row++) {
                for ($column = 0; $column < $width; $column++) {
                    $colors[] = Palette::INSIDE;
                    $glitches[] = $column;
                    $glitches[] = $row;
                }
            }

            return ['colors' => $colors, 'glitches' => $glitches];
        }
        $lastIndex = $count - 1;
        // The loop advances only while a step still fits under the limit. The
        // escape test for the final index is done after the loop, which keeps
        // the limit comparison out of the inner loop.
        $lastIter = $limit - 1;
        // A reference orbit that escaped cannot be walked past its escape; a
        // pixel that outlives it goes to the slow path instead of reusing the
        // short table, which is not accurate enough to trust.
        $guard = $count < $lastIter + 1 ? $count : PHP_INT_MAX;

        for ($row = $rowBegin; $row < $rowEnd; $row++) {
            // row 0 is the top of the image and the highest imaginary part
            $dci = -($row * $step + $step / 2 - $halfHeight) / $scale;
            for ($column = 0; $column < $width; $column++) {
                // the numerator is exact in double, so one division gives the
                // most precise dc the double range allows
                $dcr = ($column * $step + $step / 2 - $halfWidth) / $scale;
                // z(0) = Z(0) = 0 and c = c0 + dc, so the delta starts at zero;
                // seeding it with dc would add a spurious dc^2 to every pixel
                $dxr = 0.0;
                $dxi = 0.0;
                $iter = 0;
                $index = 0;
                $glitch = false;
                $direct = false;
                $directIter = 0;
                $handRe = 0.0;
                $handIm = 0.0;
                $zm = 0.0;

                while ($iter < $lastIter) {
                    if ($index >= $escapeIndex) {
                        // The reference left the set. Past that index it cannot
                        // represent a bounded pixel orbit, but z is still exact
                        // here (the last reference value has modulus below two),
                        // so finish this pixel with the direct double iteration.
                        $handRe = $zrList[$index] + $dxr;
                        $handIm = $ziList[$index] + $dxi;
                        $directIter = $iter;
                        $direct = true;
                        break;
                    }
                    if ($index >= $guard) {
                        // The reference table ends before the limit. Continue
                        // from the last value the delta iteration produced.
                        if ($iter > 0) {
                            $handRe = $zx;
                            $handIm = $zy;
                            $directIter = $iter - 1;
                            $direct = true;
                            break;
                        }
                        $glitch = true;
                        break;
                    }
                    $Zr = $zrList[$index];
                    $Zi = $ziList[$index];
                    $zx = $Zr + $dxr;
                    $zy = $Zi + $dxi;
                    $zm = $zx * $zx + $zy * $zy;
                    // Check cancellation before the escape test: once |z| is far
                    // below |Z| the computed value is rounding noise and both
                    // tests are meaningless
                    if ($zm < $cancel[$index]) {
                        $glitch = true;
                        break;
                    }
                    if ($zm > 4.0) {
                        break;
                    }
                    // rebase when the delta has outgrown the pixel value; the
                    // test is written inline to save an assignment per step
                    if ($zm < $dxr * $dxr + $dxi * $dxi) {
                        // Rebase: delta := z, then walk the reference again from
                        // index 0. This is exact because the delta carries the
                        // pixel value and the orbit is deterministic.
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

                if ($direct) {
                    // direct double iteration from the handoff value, with the
                    // pixel's own c = centre + dc. At the depths where this path
                    // is taken a double still resolves one pixel.
                    $cRe = $centreRe + $dcr;
                    $cIm = $centreIm + $dci;
                    $iter = $directIter;
                    while (true) {
                        $zm = $handRe * $handRe + $handIm * $handIm;
                        if ($zm > 4.0) {
                            break;
                        }
                        if ($iter + 1 >= $limit) {
                            $iter = $limit;
                            break;
                        }
                        $nextIm = 2.0 * $handRe * $handIm + $cIm;
                        $handRe = $handRe * $handRe - $handIm * $handIm + $cRe;
                        $handIm = $nextIm;
                        $iter++;
                    }
                } elseif (!$glitch && $iter === $lastIter) {
                    // final test point, reached when the pixel did not escape
                    $Zr = $zrList[$index];
                    $Zi = $ziList[$index];
                    $zx = $Zr + $dxr;
                    $zy = $Zi + $dxi;
                    $zm = $zx * $zx + $zy * $zy;
                    if (!($zm > 4.0)) {
                        $iter = $limit;
                        if (!($zm <= 4.0)) {
                            // the delta turned into a NaN
                            $glitch = true;
                        }
                    }
                }

                if ($glitch) {
                    $glitches[] = $column;
                    $glitches[] = $row;
                }
                if ($iter >= $limit) {
                    $colors[] = Palette::INSIDE;
                } else {
                    $colors[] = Palette::color($iter, $limit, $zm);
                }
            }
        }

        return ['colors' => $colors, 'glitches' => $glitches];
    }
}
