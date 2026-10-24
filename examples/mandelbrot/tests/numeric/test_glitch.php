<?php

declare(strict_types=1);

use Swow\Mandelbrot\Reference;
use Swow\Mandelbrot\Renderer;
use Swow\Mandelbrot\View;

/**
 * The slow path and the glitch list.
 *
 * A reference orbit that stays inside the set never glitches: the rebase rule
 * from rust-fractal-core repairs the delta, and the measured count is zero at
 * every depth from 1e4 to 1e30 (tests/numeric/bench.php). Glitches appear when
 * the reference point itself escapes, which is what a user gets by typing a
 * centre outside the set: the reference is then too short to walk, and the
 * pixels that outlive it are handed to the slow path.
 */
function testGlitch(): void
{
    echo "Glitch fallback\n";

    $view = new View(24, 14);
    $view->setDepth('1e6');
    $reference = new Reference($view);
    $reference->build();
    $params = $view->params(24, 14, 1);
    // glitches only exist on the perturbation path
    $params['path'] = 'perturb';
    $limit = $params['limit'];
    $precision = $view->precision();
    $scale = $view->scale();

    $same = 0;
    $tested = 0;
    for ($y = 0; $y < 14; $y += 4) {
        for ($x = 0; $x < 24; $x += 5) {
            [$cRe, $cIm] = pixelCenter($view, $x, $y, 1, $precision, $scale);
            [$count, $magnitude] = bcmathPixel($cRe, $cIm, $precision, $limit);
            $expected = pixelColor($count, $magnitude, $limit);
            if ($reference->renderPixelSlow($x, $y, 1, $limit) === $expected) {
                $same++;
            }
            $tested++;
        }
    }
    checkSame($tested, $same, 'renderPixelSlow matches an independent bcmath iteration at 1e6');

    // a band with no reference orbit at all must hand every pixel to the slow path
    $result = Renderer::renderBand(['zr' => [], 'zi' => [], 'mag' => []], $params, 0, 14);
    checkSame(24 * 14, count($result['colors']), 'empty orbit band covers the image');
    checkSame(24 * 14 * 2, count($result['glitches']), 'empty orbit reports every pixel once');
    $repaired = repairGlitches($result['colors'], $result['glitches'], $reference, 24, $limit);
    $same = 0;
    for ($y = 0; $y < 14; $y++) {
        for ($x = 0; $x < 24; $x++) {
            [$cRe, $cIm] = pixelCenter($view, $x, $y, 1, $precision, $scale);
            [$count, $magnitude] = bcmathPixel($cRe, $cIm, $precision, $limit);
            if ($repaired[$y * 24 + $x] === pixelColor($count, $magnitude, $limit)) {
                $same++;
            }
        }
    }
    checkSame(24 * 14, $same, 'repairing an empty orbit band reproduces the exact image');

    // a natural glitch: this centre makes one pixel pass within 1e-6 of zero
    // while the reference is far from it, which is the cancellation the
    // Pauldelbrot test from numeric.md detects
    $natural = new View(24, 14);
    $natural->setCenter('-0.1', '0.9');
    $natural->setDepth('1');
    $naturalReference = new Reference($natural);
    $naturalReference->build();
    $naturalParams = $natural->params(24, 14, 1);
    $forced = $naturalParams;
    $forced['path'] = 'perturb';
    $result = Renderer::renderBand($naturalReference->orbit(), $forced, 0, 14);
    $glitchPixels = intdiv(count($result['glitches']), 2);
    $repaired = repairGlitches($result['colors'], $result['glitches'], $naturalReference, 24, $naturalParams['limit']);
    $exact = 0;
    for ($i = 0; $i < count($result['glitches']); $i += 2) {
        $x = $result['glitches'][$i];
        $y = $result['glitches'][$i + 1];
        [$cRe, $cIm] = pixelCenter($natural, $x, $y, 1, $natural->precision(), $natural->scale());
        [$count, $magnitude] = bcmathPixel($cRe, $cIm, $natural->precision(), $naturalParams['limit']);
        if ($repaired[$y * 24 + $x] === pixelColor($count, $magnitude, $naturalParams['limit'])) {
            $exact++;
        }
    }
    printf("  natural cancellation at -0.1+0.9i depth 1: %d glitches, %d repaired exactly\n", $glitchPixels, $exact);
    check($glitchPixels > 0, 'a natural cancellation produces a glitch');
    checkSame($glitchPixels, $exact, 'the natural glitch is repaired exactly');

    // a shortened reference orbit stands for a reference point that escapes
    // before the limit: pixels past its end continue with the direct double
    // iteration, so the band must equal the direct path and report no glitches
    $orbit = $reference->orbit();
    $short = ['zr' => array_slice($orbit['zr'], 0, 6), 'zi' => array_slice($orbit['zi'], 0, 6), 'mag' => array_slice($orbit['mag'], 0, 6)];
    $shortResult = Renderer::renderBand($short, $params, 0, 14);
    $directParams = $params;
    $directParams['path'] = 'direct';
    $directResult = Renderer::renderBand($orbit, $directParams, 0, 14);
    $sameAsDirect = 0;
    foreach ($shortResult['colors'] as $i => $color) {
        if ($color === $directResult['colors'][$i]) {
            $sameAsDirect++;
        }
    }
    printf("  reference orbit cut to 6 entries: %d glitches, %d of %d equal the direct path\n",
        intdiv(count($shortResult['glitches']), 2), $sameAsDirect, 24 * 14);
    checkSame(0, intdiv(count($shortResult['glitches']), 2), 'a short reference continues directly instead of glitching');
    // the handoff starts from z at the reference end, not from zero, so a
    // chaotic frame can differ on a few boundary pixels
    check($sameAsDirect >= 24 * 14 - 8, 'the direct continuation reproduces the direct path');

    // a centre outside the set makes the reference orbit escape after a couple
    // of steps: every pixel reaches that index and continues directly
    $outside = new View(32, 20);
    $outside->setCenter('3', '0');
    $outside->setDepth('1');
    $outsideReference = new Reference($outside);
    $outsideReference->build();
    $outsideParams = $outside->params(32, 20, 1);
    $outsidePerturb = $outsideParams;
    $outsidePerturb['path'] = 'perturb';
    $outsideDirect = $outsideParams;
    $outsideDirect['path'] = 'direct';
    $outsideResult = Renderer::renderBand($outsideReference->orbit(), $outsidePerturb, 0, 20);
    $outsideDirectResult = Renderer::renderBand($outsideReference->orbit(), $outsideDirect, 0, 20);
    $outsideSame = 0;
    foreach ($outsideResult['colors'] as $i => $color) {
        if ($color === $outsideDirectResult['colors'][$i]) {
            $outsideSame++;
        }
    }
    printf("  escaped reference (centre 3+0i, depth 1): %d glitches, %d of %d equal the direct path\n",
        intdiv(count($outsideResult['glitches']), 2), $outsideSame, 32 * 20);
    checkSame(0, intdiv(count($outsideResult['glitches']), 2), 'an escaped reference produces no glitches');
    checkSame(32 * 20, $outsideSame, 'an escaped reference continues with the direct iteration');
}
