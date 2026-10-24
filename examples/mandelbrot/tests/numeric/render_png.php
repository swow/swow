<?php

declare(strict_types=1);

/**
 * Renders fixed frames with the numeric core alone (no App, no workers) and
 * writes PNGs through src/Image.php.
 *
 * The two frames are the ones used to settle whether the 1e10 picture is
 * speckle or real structure: the default centre at depth 1e4 (shallow, direct
 * path) and at depth 1e10 (perturbation path).
 *
 *   php tests/numeric/render_png.php
 *   php tests/numeric/render_png.php 1e10
 */

require __DIR__ . '/bootstrap.php';
require __DIR__ . '/support.php';

use Swow\Mandelbrot\Image;
use Swow\Mandelbrot\Palette;
use Swow\Mandelbrot\Reference;
use Swow\Mandelbrot\Renderer;
use Swow\Mandelbrot\View;

$depths = $argv[1] ?? null;
$depths = $depths === null ? ['1e4', '1e10'] : [$depths];
$width = 128;
$height = 72;
$outDir = __DIR__ . '/out';
if (!is_dir($outDir) && !mkdir($outDir, 0o777, true)) {
    fwrite(STDERR, "cannot create {$outDir}\n");
    exit(1);
}

foreach ($depths as $depth) {
    $view = new View($width, $height);
    $view->setDepth($depth);
    $reference = new Reference($view);
    $reference->build();
    $params = $view->params($width, $height, 1);
    $start = hrtime(true);
    $result = Renderer::renderBand($reference->orbit(), $params, 0, $height);
    $ms = (hrtime(true) - $start) / 1e6;

    // BGRA with a fixed opaque alpha, the layout the shared buffer and
    // Image::save() expect
    $bgra = '';
    $inside = 0;
    $colours = [];
    foreach ($result['colors'] as $color) {
        if ($color === Palette::INSIDE) {
            $inside++;
        }
        $colours[$color] = true;
        $bgra .= chr($color & 0xFF) . chr(($color >> 8) & 0xFF) . chr(($color >> 16) & 0xFF) . "\xFF";
    }

    $path = $outDir . '/mandelbrot-' . $depth . '.png';
    Image::save($path, $bgra, $width, $height);
    $png = file_get_contents($path);

    printf(
        "depth %-5s limit %4d  render %6.1f ms  glitches %d  inside %d/%d  colours %d\n",
        $depth,
        $params['limit'],
        $ms,
        intdiv(count($result['glitches']), 2),
        $inside,
        $width * $height,
        count($colours)
    );
    printf("  colour digest xxh3 %s\n", digestColors($result['colors']));
    printf("  png sha256 %s  size %d bytes  %s\n", hash('sha256', $png), strlen($png), $path);

    // coarse preview, one character per 2x4 block, darkest first
    $shades = ' .:-=+*#%@';
    for ($y = 0; $y < $height; $y += 4) {
        $line = '';
        for ($x = 0; $x < $width; $x += 2) {
            $color = $result['colors'][$y * $width + $x];
            $luma = ((($color >> 16) & 0xFF) * 2 + (($color >> 8) & 0xFF) * 3 + ($color & 0xFF)) / 6;
            $line .= $shades[(int) min(9, $luma / 25.6)];
        }
        echo '  ', $line, "\n";
    }
}
