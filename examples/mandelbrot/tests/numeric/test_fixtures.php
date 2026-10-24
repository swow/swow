<?php

declare(strict_types=1);

use Swow\Mandelbrot\Palette;
use Swow\Mandelbrot\Reference;
use Swow\Mandelbrot\Renderer;
use Swow\Mandelbrot\View;

/**
 * Regression fixtures for the numeric core.
 *
 * The digests are xxh3 over the flat 0x00RRGGBB row major colour list. They
 * depend on the palette's libm results, so they also pin the code path that
 * produced them: a change in the iteration, the reference orbit, the palette or
 * the pixel offset changes them. The deep coordinate keeps structure at 1e20
 * and 1e30, where the default centre is legitimately flat.
 *
 * The second half samples 32 pixels per depth and compares each one against an
 * independent bcmath iteration, which is the authority the digests cannot be.
 */
function testFixtures(): void
{
    echo "Fixtures\n";
    $frames = [
        // [width, height, depth, centre (null = default), digest, inside, path]
        [128, 72, '1e4', null, 'a7ee7527248af3f6', 2, 'direct'],
        [128, 72, '1e8', null, '539b55e00b4b7502', 827, 'direct'],
        [128, 72, '1e10', null, '479bc1afa80dffc4', 5, 'perturb'],
        [64, 36, '1e20', 'deep', '89063b74e10beb41', 29, 'perturb'],
        [64, 36, '1e30', 'deep', '4eb7c7f80e371505', 0, 'perturb'],
    ];
    foreach ($frames as [$width, $height, $depth, $centre, $expected, $expectedInside, $expectedPath]) {
        $view = new View($width, $height);
        if ($centre === 'deep') {
            $view->setCenter(DEEP_CENTER_RE, DEEP_CENTER_IM);
        }
        $view->setDepth($depth);
        $reference = new Reference($view);
        $reference->build();
        $params = $view->params($width, $height, 1);
        // the auto rule is part of the fixture: 1e10 must still perturb
        $unit = 300.0 * ($params['fullHeight'] / 1080.0);
        $path = $params['scale'] < $unit * Renderer::DIRECT_DEPTH ? 'direct' : 'perturb';
        checkSame($expectedPath, $path, "{$width}x{$height} {$depth} auto path");
        $start = hrtime(true);
        $result = Renderer::renderBand($reference->orbit(), $params, 0, $height);
        $ms = (hrtime(true) - $start) / 1e6;
        $inside = 0;
        foreach ($result['colors'] as $color) {
            if ($color === Palette::INSIDE) {
                $inside++;
            }
        }
        printf("  %dx%d %-5s limit %d: %.0f ms, %d colours, %d inside\n",
            $width, $height, $depth, $params['limit'], $ms, count(array_unique($result['colors'])), $inside);
        checkSame($width * $height, count($result['colors']), "{$width}x{$height} {$depth} band length");
        checkSame([], $result['glitches'], "{$width}x{$height} {$depth} reports no glitches");
        checkSame($expectedInside, $inside, "{$width}x{$height} {$depth} inside pixel count");
        checkSame($expected, digestColors($result['colors']), "{$width}x{$height} {$depth} digest");
    }

    // full frames render the perturbation path at every depth now, so pin the
    // shallow perturbation image separately from the auto direct fixture above
    $view = new View(128, 72);
    $view->setDepth('1e4');
    $reference = new Reference($view);
    $reference->build();
    $params = $view->params(128, 72, 1);
    $params['path'] = 'perturb';
    $start = hrtime(true);
    $result = Renderer::renderBand($reference->orbit(), $params, 0, 72);
    $ms = (hrtime(true) - $start) / 1e6;
    $inside = 0;
    foreach ($result['colors'] as $color) {
        if ($color === Palette::INSIDE) {
            $inside++;
        }
    }
    printf("  128x72 1e4 forced perturb: %.0f ms, %d colours, %d inside\n", $ms, count(array_unique($result['colors'])), $inside);
    checkSame(128 * 72, count($result['colors']), '128x72 1e4 perturb band length');
    checkSame([], $result['glitches'], '128x72 1e4 perturb reports no glitches');
    checkSame(2, $inside, '128x72 1e4 perturb inside pixel count');
    checkSame('3d761eac9fe9c621', digestColors($result['colors']), '128x72 1e4 perturb digest');

    // 32 pixels spread over the frame, exact colour equality against bcmath
    $cases = [
        // [width, height, depth, centre]
        [64, 36, '1e4', null],
        [64, 36, '1e5', null],
        [64, 36, '1e6', null],
        [64, 36, '1e6', 'deep'],
        [64, 36, '1e10', null],
        [64, 36, '1e10', 'deep'],
        [64, 36, '1e30', 'deep'],
    ];
    foreach ($cases as [$width, $height, $depth, $centre]) {
        $view = new View($width, $height);
        if ($centre === 'deep') {
            $view->setCenter(DEEP_CENTER_RE, DEEP_CENTER_IM);
        }
        $view->setDepth($depth);
        $reference = new Reference($view);
        $reference->build();
        $params = $view->params($width, $height, 1);
        // The sampled gate is for the perturbation numerics and is used by the
        // App only at depth >= DIRECT_DEPTH, where the auto rule perturbs. The
        // direct path is the fast path and differs from bcmath by a sub-pixel
        // position at boundary pixels, so force the perturbation here.
        $sampleParams = $params;
        $sampleParams['path'] = 'perturb';
        $result = Renderer::renderBand($reference->orbit(), $sampleParams, 0, $height);
        $precision = $view->precision();
        $scale = $view->scale();
        $exact = 0;
        $tested = 0;
        for ($k = 0; $k < 32; $k++) {
            $x = ($k * 31 + 7) % $width;
            $y = ($k * 17 + 3) % $height;
            [$cRe, $cIm] = pixelCenter($view, $x, $y, 1, $precision, $scale);
            [$count, $magnitude] = bcmathPixel($cRe, $cIm, $precision, $params['limit']);
            if ($result['colors'][$y * $width + $x] === pixelColor($count, $magnitude, $params['limit'])) {
                $exact++;
            } else {
                printf("  mismatch at (%d,%d) depth %s\n", $x, $y, $depth);
            }
            $tested++;
        }
        checkSame($tested, $exact, "32 sampled pixels at {$depth} " . ($centre === 'deep' ? 'deep centre' : 'default centre') . ' match bcmath exactly');
    }
}
