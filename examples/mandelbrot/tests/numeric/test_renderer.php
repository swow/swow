<?php

declare(strict_types=1);

use Swow\Mandelbrot\Reference;
use Swow\Mandelbrot\Renderer;
use Swow\Mandelbrot\View;

function testRenderer(): void
{
    echo "Renderer\n";

    // fixed small view through the direct path, digest recorded once
    $view = new View(160, 90);
    $view->setDepth('1e4');
    $reference = new Reference($view);
    $reference->build();
    $params = $view->params(160, 90, 1);
    $result = Renderer::renderBand($reference->orbit(), $params, 0, 90);
    checkSame(160 * 90, count($result['colors']), 'band covers the whole target image');
    checkSame([], $result['glitches'], 'direct path reports no glitches');
    checkSame('b8cbc475f86e0942', digestColors($result['colors']), '160x90 at 1e4 has the recorded digest');
    check(count(array_unique($result['colors'])) > 100, '160x90 at 1e4 shows structure');

    // the digest must not depend on how the frame is split into bands
    $split = [];
    foreach ([[0, 30], [30, 61], [61, 90]] as [$begin, $end]) {
        $part = Renderer::renderBand($reference->orbit(), $params, $begin, $end);
        checkSame(($end - $begin) * 160, count($part['colors']), "band {$begin}..{$end} length");
        foreach ($part['colors'] as $color) {
            $split[] = $color;
        }
    }
    checkSame(digestColors($result['colors']), digestColors($split), 'direct path is band split invariant');

    // perturbation path, same invariants at a depth that needs it
    $deep = new View(40, 22);
    $deep->setDepth('1e10');
    $deepReference = new Reference($deep);
    $deepReference->build();
    $deepParams = $deep->params(40, 22, 1);
    $full = Renderer::renderBand($deepReference->orbit(), $deepParams, 0, 22);
    checkSame(40 * 22, count($full['colors']), 'perturbation band length');
    checkSame([], $full['glitches'], 'perturbation reports no glitches at 1e10');
    $split = [];
    foreach ([[0, 5], [5, 17], [17, 22]] as [$begin, $end]) {
        $part = Renderer::renderBand($deepReference->orbit(), $deepParams, $begin, $end);
        foreach ($part['colors'] as $color) {
            $split[] = $color;
        }
    }
    checkSame(digestColors($full['colors']), digestColors($split), 'perturbation path is band split invariant');
    check(count(array_unique($full['colors'])) > 20, 'perturbation at 1e10 shows structure');

    // the optional path flag overrides the depth rule
    $forced = $deepParams;
    $forced['path'] = 'perturb';
    checkSame(digestColors($full['colors']), digestColors(Renderer::renderBand($deepReference->orbit(), $forced, 0, 22)['colors']), 'path=perturb is honoured');
    $forced['path'] = 'direct';
    $directAtDeep = Renderer::renderBand($deepReference->orbit(), $forced, 0, 22);
    checkSame(40 * 22, count($directAtDeep['colors']), 'path=direct is honoured below and above the threshold');
    $shallow = new View(24, 14);
    $shallow->setDepth('1e4');
    $shallowReference = new Reference($shallow);
    $shallowReference->build();
    $shallowParams = $shallow->params(24, 14, 1);
    $auto = Renderer::renderBand($shallowReference->orbit(), $shallowParams, 0, 14);
    $forcedPerturb = $shallowParams;
    $forcedPerturb['path'] = 'perturb';
    $perturbShallow = Renderer::renderBand($shallowReference->orbit(), $forcedPerturb, 0, 14);
    $close = 0;
    foreach ($auto['colors'] as $i => $color) {
        if ($color === $perturbShallow['colors'][$i]) {
            $close++;
        }
    }
    check($close >= 24 * 14 - 2, "perturbation below 1e6 agrees with the direct path ({$close}/" . (24 * 14) . ')');
    $forcedAuto = $shallowParams;
    $forcedAuto['path'] = 'auto';
    checkSame(digestColors($auto['colors']), digestColors(Renderer::renderBand($shallowReference->orbit(), $forcedAuto, 0, 14)['colors']), 'path=auto is the default');

    // preview step: the target size shrinks and rows are still row major
    $previewParams = $view->params(160, 90, 4);
    checkSame(40, $previewParams['width'], 'preview width');
    checkSame(22, $previewParams['height'], 'preview height');
    $preview = Renderer::renderBand($reference->orbit(), $previewParams, 0, $previewParams['height']);
    checkSame(40 * 22, count($preview['colors']), 'preview band length');
    checkSame(160, $params['fullWidth'], 'full width stays in params');

    // empty orbit: nothing to walk, every pixel must be reported for the slow path
    $empty = Renderer::renderBand(['zr' => [], 'zi' => [], 'mag' => []], $deepParams, 0, 2);
    checkSame(2 * 40, count($empty['colors']), 'empty orbit band length');
    checkSame(2 * 40 * 2, count($empty['glitches']), 'empty orbit reports every pixel as a glitch');
    $inRange = true;
    for ($i = 0; $i < count($empty['glitches']); $i += 2) {
        if ($empty['glitches'][$i] < 0 || $empty['glitches'][$i] >= 40 || $empty['glitches'][$i + 1] < 0 || $empty['glitches'][$i + 1] >= 2) {
            $inRange = false;
        }
    }
    check($inRange, 'glitch coordinates are target image coordinates');

    // the pixel offset convention from contracts.md, checked through the direct path
    $one = new View(1, 1);
    $one->setCenter('0', '0');
    $one->setDepth('1e4');
    $oneReference = new Reference($one);
    $oneReference->build();
    $oneParams = $one->params(1, 1, 1);
    $color = Renderer::renderBand($oneReference->orbit(), $oneParams, 0, 1)['colors'][0];
    // pixel (0,0) of a 1x1 image sits at centre + (0 + 0.5 - 0.5) / scale = centre
    [$count, $magnitude] = bcmathPixel('0', '0', $one->precision(), $oneParams['limit']);
    checkSame(pixelColor($count, $magnitude, $oneParams['limit']), $color, 'a one pixel image samples the centre');
}
