<?php

declare(strict_types=1);

/**
 * Performance and glitch measurements for the numeric core.
 *
 * Run directly, it prints a table and exits 0. It is intentionally not part of
 * run.php because a 320x180 frame at depth 1e20 takes tens of seconds.
 *
 *   php tests/numeric/bench.php
 */

require __DIR__ . '/bootstrap.php';

use Swow\Mandelbrot\Reference;
use Swow\Mandelbrot\Renderer;
use Swow\Mandelbrot\View;

/** 64 bit FNV-1a over the packed colours, for run to run comparison. */
function bandDigest(array $colors): string
{
    $bytes = '';
    foreach ($colors as $color) {
        $bytes .= chr(($color >> 16) & 0xFF) . chr(($color >> 8) & 0xFF) . chr($color & 0xFF);
    }
    // hash() keeps the digest identical across 32 and 64 bit builds
    return hash('xxh3', $bytes);
}

$depths = [1e4, 1e6, 1e8, 1e10, 1e20, 1e30];

echo "Reference::build()\n";
printf("%-10s %7s %5s %10s %10s\n", 'depth', 'limit', 'prec', 'build ms', 'orbit B');
foreach ($depths as $depth) {
    $view = new View(1280, 676);
    $view->setDepth((string) $depth);
    $reference = new Reference($view);
    $start = hrtime(true);
    $reference->build();
    $ms = (hrtime(true) - $start) / 1e6;
    $orbit = $reference->orbit();
    $bytes = 0;
    foreach (['zr', 'zi', 'mag'] as $key) {
        $bytes += count($orbit[$key]) * 8;
    }
    printf("%-10s %7d %5d %10.2f %10d\n", (string) $depth, $view->limit(), $view->precision(), $ms, $bytes);
}

echo "\nBand render, 320x180, step 1, colors per pixel\n";
printf("%-8s %6s %9s %8s %10s %9s %8s %8s\n", 'depth', 'limit', 'path', 'ms', 'us/px', 'px/s', 'glitch', 'digest');
foreach ([1e6, 1e10, 1e20] as $depth) {
    $width = 320;
    $height = 180;
    $view = new View($width, $height);
    $view->setDepth((string) $depth);
    $reference = new Reference($view);
    $reference->build();
    $params = $view->params($width, $height, 1);
    $orbit = $reference->orbit();
    $rows = 12;
    $bandHeight = intdiv($height, $rows);
    $start = hrtime(true);
    $colors = [];
    $glitches = 0;
    for ($band = 0; $band < $rows; $band++) {
        $rowBegin = $band * $bandHeight;
        $rowEnd = $band === $rows - 1 ? $height : $rowBegin + $bandHeight;
        $result = Renderer::renderBand($orbit, $params, $rowBegin, $rowEnd);
        foreach ($result['colors'] as $color) {
            $colors[] = $color;
        }
        $glitches += count($result['glitches']) / 2;
    }
    $ms = (hrtime(true) - $start) / 1e6;
    $pixels = $width * $height;
    printf("%-8s %6d %9s %8.1f %10.2f %9.0f %8d %8s\n", (string) $depth, $params['limit'], 'perturb', $ms,
        $ms * 1000 / $pixels, $pixels / ($ms / 1000), $glitches, bandDigest($colors));

    if ($depth === 1e6) {
        // the same frame through the direct double loop, outside the production
        // threshold so the two paths can be compared at one depth
        $start = hrtime(true);
        $direct = directDoubleBand($params, 0, $height);
        $ms = (hrtime(true) - $start) / 1e6;
        $same = $direct === $colors ? 'identical' : 'differs';
        printf("%-8s %6d %9s %8.1f %10.2f %9.0f %8d %8s %s\n", (string) $depth, $params['limit'], 'direct', $ms,
            $ms * 1000 / $pixels, $pixels / ($ms / 1000), 0, bandDigest($direct), $same);
    }
}

echo "\nGlitch rate and slow path, 64x36\n";
printf("%-8s %6s %10s %8s %14s %16s\n", 'depth', 'limit', 'render ms', 'glitch', 'slow us/px', 'slow full frame s');
foreach ([1e10, 1e20, 1e30] as $depth) {
    $width = 64;
    $height = 36;
    $view = new View($width, $height);
    $view->setDepth((string) $depth);
    $reference = new Reference($view);
    $reference->build();
    $params = $view->params($width, $height, 1);
    $start = hrtime(true);
    $result = Renderer::renderBand($reference->orbit(), $params, 0, $height);
    $ms = (hrtime(true) - $start) / 1e6;
    $glitchPixels = intdiv(count($result['glitches']), 2);
    $samples = min(8, $glitchPixels);
    $slowUs = 0.0;
    if ($samples > 0) {
        $start = hrtime(true);
        for ($i = 0; $i < $samples; $i++) {
            $x = $result['glitches'][$i * 2];
            $y = $result['glitches'][$i * 2 + 1];
            $reference->renderPixelSlow($x, $y, 1, $params['limit']);
        }
        $slowUs = (hrtime(true) - $start) / 1e3 / $samples;
    } else {
        // no glitch to time, measure one known pixel at the same view
        $start = hrtime(true);
        $reference->renderPixelSlow(0, 0, 1, $params['limit']);
        $slowUs = (hrtime(true) - $start) / 1e3;
    }
    $framePixels = 1280 * 676;
    printf("%-8s %6d %10.2f %8d %14.1f %16.1f\n", (string) $depth, $params['limit'], $ms, $glitchPixels, $slowUs,
        $slowUs * $framePixels * $glitchPixels / max(1, $width * $height) / 1e6);
}

/**
 * Direct double iteration of a whole band, for the 1e6 comparison only.
 *
 * @param array{centerRe: string, centerIm: string, scale: float, limit: int,
 *     fullWidth: int, fullHeight: int, width: int, height: int, step: int} $params
 * @return list<int>
 */
function directDoubleBand(array $params, int $rowBegin, int $rowEnd): array
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
            $colors[] = $n >= $limit ? 0 : Swow\Mandelbrot\Palette::color($n, $limit, $zr2 + $zi2);
        }
    }

    return $colors;
}
