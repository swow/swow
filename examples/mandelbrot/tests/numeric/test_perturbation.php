<?php

declare(strict_types=1);

use Swow\Mandelbrot\Reference;
use Swow\Mandelbrot\Renderer;
use Swow\Mandelbrot\View;

/**
 * Pixels the double path cannot decide: the direct double iteration disagrees
 * with the exact bcmath orbit for the same pixel.
 */
function doubleLimited(View $view, int $x, int $y, int $step, array $params): bool
{
    $scale = $params['scale'];
    $precision = $view->precision();
    [$cRe, $cIm] = pixelCenter($view, $x, $y, $step, $precision, $scale);
    [$count, $magnitude] = bcmathPixel($cRe, $cIm, $precision, $params['limit']);
    $exact = pixelColor($count, $magnitude, $params['limit']);
    $doubleRe = (float) $view->centerRe() + ($x * $step + $step / 2 - $view->width() / 2) / $scale;
    $doubleIm = (float) $view->centerIm() + ($y * $step + $step / 2 - $view->height() / 2) / $scale;
    [$doubleCount, $doubleMagnitude] = doublePixel($doubleRe, $doubleIm, $params['limit']);

    return pixelColor($doubleCount, $doubleMagnitude, $params['limit']) !== $exact;
}

function testPerturbation(): void
{
    echo "Perturbation\n";

    // every pixel at 1e6 against exact bcmath; a mismatch is only accepted when
    // the direct double path is wrong for the same pixel too
    $view = new View(32, 18);
    $view->setDepth('1e6');
    $reference = new Reference($view);
    $reference->build();
    $params = $view->params(32, 18, 1);
    // the auto rule is direct below 1e10, so force the perturbation here
    $params['path'] = 'perturb';
    $result = Renderer::renderBand($reference->orbit(), $params, 0, 18);
    checkSame([], $result['glitches'], '1e6 reports no glitches');
    $mismatch = 0;
    $limitedMismatch = 0;
    $firstMismatch = null;
    for ($y = 0; $y < 18; $y++) {
        for ($x = 0; $x < 32; $x++) {
            [$cRe, $cIm] = pixelCenter($view, $x, $y, 1, $view->precision(), $view->scale());
            [$count, $magnitude] = bcmathPixel($cRe, $cIm, $view->precision(), $params['limit']);
            $expected = pixelColor($count, $magnitude, $params['limit']);
            $actual = $result['colors'][$y * 32 + $x];
            if ($actual === $expected) {
                continue;
            }
            $mismatch++;
            $firstMismatch ??= "({$x},{$y}) exact " . sprintf('%06x', $expected) . ' got ' . sprintf('%06x', $actual);
            if (doubleLimited($view, $x, $y, 1, $params)) {
                $limitedMismatch++;
            }
        }
    }
    printf("  1e6: %d of %d pixels differ from bcmath, %d of those are double limited%s\n", $mismatch, 32 * 18, $limitedMismatch,
        $firstMismatch === null ? '' : ", first {$firstMismatch}");
    check($mismatch <= 2, '1e6 matches exact bcmath on all but at most two pixels');
    checkSame($mismatch, $limitedMismatch, 'every 1e6 mismatch is a pixel double precision cannot decide');

    // 1e10: the depth S1 reported as speckle. Check a fixed sample exactly.
    $view = new View(64, 36);
    $view->setDepth('1e10');
    $reference = new Reference($view);
    $reference->build();
    $params = $view->params(64, 36, 1);
    $result = Renderer::renderBand($reference->orbit(), $params, 0, 36);
    checkSame([], $result['glitches'], '1e10 reports no glitches');
    check(count(array_unique($result['colors'])) > 50, '1e10 frame is not a flat or random image');
    mt_srand(1013);
    $tested = 0;
    $same = 0;
    for ($k = 0; $k < 40; $k++) {
        $x = mt_rand(0, 63);
        $y = mt_rand(0, 35);
        [$cRe, $cIm] = pixelCenter($view, $x, $y, 1, $view->precision(), $view->scale());
        [$count, $magnitude] = bcmathPixel($cRe, $cIm, $view->precision(), $params['limit']);
        $expected = pixelColor($count, $magnitude, $params['limit']);
        if ($result['colors'][$y * 64 + $x] === $expected) {
            $same++;
        }
        $tested++;
    }
    checkSame($tested, $same, '1e10 sample matches exact bcmath');

    // deep view with structure: the default centre is about 1e-15 from the
    // boundary, so past that the frame is legitimately flat. This coordinate
    // keeps structure at 1e30.
    $view = new View(64, 36);
    $view->setCenter(DEEP_CENTER_RE, DEEP_CENTER_IM);
    $view->setDepth('1e30');
    $reference = new Reference($view);
    $reference->build();
    $params = $view->params(64, 36, 1);
    $result = Renderer::renderBand($reference->orbit(), $params, 0, 36);
    checkSame([], $result['glitches'], '1e30 reports no glitches');
    $inside = 0;
    foreach ($result['colors'] as $color) {
        if ($color === 0) {
            $inside++;
        }
    }
    printf("  1e30 deep coordinate: %d inside of %d, %d colours, limit %d\n", $inside, 64 * 36,
        count(array_unique($result['colors'])), $params['limit']);
    check(count(array_unique($result['colors'])) >= 50, '1e30 deep coordinate shows structure');
    mt_srand(773);
    $tested = 0;
    $same = 0;
    for ($k = 0; $k < 30; $k++) {
        $x = mt_rand(0, 63);
        $y = mt_rand(0, 35);
        [$cRe, $cIm] = pixelCenter($view, $x, $y, 1, $view->precision(), $view->scale());
        [$count, $magnitude] = bcmathPixel($cRe, $cIm, $view->precision(), $params['limit']);
        $expected = pixelColor($count, $magnitude, $params['limit']);
        if ($result['colors'][$y * 64 + $x] === $expected) {
            $same++;
        }
        $tested++;
    }
    checkSame($tested, $same, '1e30 deep sample matches exact bcmath');

    // orientation: row 0 of the image is the highest imaginary part. The mask
    // is built twice from the same double iteration, once with the standard row
    // mapping and once with the vertical mirror; only the standard one may match.
    $flat = new View(16, 12);
    $flat->setDepth('1');
    $flatReference = new Reference($flat);
    $flatReference->build();
    $flatParams = $flat->params(16, 12, 1);
    $flatDirect = Renderer::renderBand($flatReference->orbit(), $flatParams + ['path' => 'direct'], 0, 12);
    $flatPerturb = Renderer::renderBand($flatReference->orbit(), $flatParams + ['path' => 'perturb'], 0, 12);
    $flatScale = $flat->scale();
    $flatLimit = $flatParams['limit'];
    $flatRe = (float) $flat->centerRe();
    $flatIm = (float) $flat->centerIm();
    $standard = 0;
    $mirrored = 0;
    $perturbSame = 0;
    $total = 16 * 12;
    for ($y = 0; $y < 12; $y++) {
        for ($x = 0; $x < 16; $x++) {
            $index = $y * 16 + $x;
            $renderedInside = $flatDirect['colors'][$index] === 0;
            $cRe = $flatRe + ($x + 0.5 - 8) / $flatScale;
            // standard: row 0 is the highest imaginary part
            [$standardCount] = doublePixel($cRe, $flatIm - ($y + 0.5 - 6) / $flatScale, $flatLimit);
            [$mirroredCount] = doublePixel($cRe, $flatIm + ($y + 0.5 - 6) / $flatScale, $flatLimit);
            if (($standardCount >= $flatLimit) === $renderedInside) {
                $standard++;
            }
            if (($mirroredCount >= $flatLimit) === $renderedInside) {
                $mirrored++;
            }
            if (($flatPerturb['colors'][$index] === 0) === $renderedInside) {
                $perturbSame++;
            }
        }
    }
    printf("  orientation 16x12 at depth 1: standard row mapping agrees on %d/%d pixels, mirror on %d/%d\n",
        $standard, $total, $mirrored, $total);
    checkSame($total, $standard, 'row 0 is the highest imaginary part');
    check($mirrored < $total - 8, 'the vertically mirrored mapping does not match');
    checkSame($total, $perturbSame, 'the perturbation path uses the same row mapping as the direct path');

    // the --verify comparison at 1e5: the direct path is the less accurate one,
    // so a small mismatch fraction is expected and bounded here
    $view = new View(64, 36);
    $view->setDepth('1e5');
    $reference = new Reference($view);
    $reference->build();
    $params = $view->params(64, 36, 1);
    $directParams = $params;
    $directParams['path'] = 'direct';
    $perturbParams = $params;
    $perturbParams['path'] = 'perturb';
    $direct = Renderer::renderBand($reference->orbit(), $directParams, 0, 36);
    $perturb = Renderer::renderBand($reference->orbit(), $perturbParams, 0, 36);
    $mismatch = 0;
    $maxChannel = 0;
    $perturbBetter = 0;
    $directBetter = 0;
    for ($y = 0; $y < 36; $y++) {
        for ($x = 0; $x < 64; $x++) {
            $i = $y * 64 + $x;
            if ($direct['colors'][$i] === $perturb['colors'][$i]) {
                continue;
            }
            $mismatch++;
            $a = $direct['colors'][$i];
            $b = $perturb['colors'][$i];
            $maxChannel = max(
                $maxChannel,
                abs((($a >> 16) & 0xFF) - (($b >> 16) & 0xFF)),
                abs((($a >> 8) & 0xFF) - (($b >> 8) & 0xFF)),
                abs(($a & 0xFF) - ($b & 0xFF))
            );
            [$cRe, $cIm] = pixelCenter($view, $x, $y, 1, $view->precision(), $view->scale());
            [$count, $magnitude] = bcmathPixel($cRe, $cIm, $view->precision(), $params['limit']);
            $exact = pixelColor($count, $magnitude, $params['limit']);
            if ($b === $exact) {
                $perturbBetter++;
            } elseif ($a === $exact) {
                $directBetter++;
            }
        }
    }
    $fraction = 100.0 * $mismatch / (64 * 36);
    printf("  1e5 direct vs perturb: %d of %d differ (%.3f%%), max channel delta %d, perturb matches bcmath on %d, direct on %d\n",
        $mismatch, 64 * 36, $fraction, $maxChannel, $perturbBetter, $directBetter);
    check($fraction < 1.5, '1e5 direct and perturbation agree on at least 98.5 percent of pixels');
    check($perturbBetter >= $directBetter, 'at 1e5 the perturbation is at least as close to bcmath as the direct path');
}
