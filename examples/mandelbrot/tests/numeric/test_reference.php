<?php

declare(strict_types=1);

use Swow\Mandelbrot\Reference;
use Swow\Mandelbrot\View;

/** Independent bcmath orbit, used only as the expected value. */
function bcmathOrbit(string $cRe, string $cIm, int $precision, int $limit): array
{
    $zr = '0';
    $zi = '0';
    $zrList = [0.0];
    $ziList = [0.0];
    for ($n = 1; $n <= $limit; $n++) {
        $nextZr = bcadd(bcsub(bcmul($zr, $zr, $precision), bcmul($zi, $zi, $precision), $precision), $cRe, $precision);
        $nextZi = bcadd(bcmul(bcmul('2', $zr, $precision), $zi, $precision), $cIm, $precision);
        $zr = $nextZr;
        $zi = $nextZi;
        $zrList[$n] = (float) $zr;
        $ziList[$n] = (float) $zi;
    }

    return [$zrList, $ziList];
}

function testReference(): void
{
    echo "Reference\n";
    $view = new View(64, 36);
    $view->setDepth('1e4');
    $reference = new Reference($view);
    checkSame(25, $reference->precision(), 'reference precision follows the view');

    $version = $reference->build();
    checkSame(1, $version, 'first build returns version 1');
    $orbit = $reference->orbit();
    $limit = $view->limit();
    checkSame(['zr', 'zi', 'mag'], array_keys($orbit), 'orbit keys');
    checkSame($limit + 1, count($orbit['zr']), 'orbit has limit + 1 entries');
    checkSame($limit + 1, count($orbit['zi']), 'orbit has limit + 1 entries');
    checkSame($limit + 1, count($orbit['mag']), 'orbit has limit + 1 entries');
    checkNear(0.0, $orbit['zr'][0], 0.0, 'index 0 real part is zero');
    checkNear(0.0, $orbit['zi'][0], 0.0, 'index 0 imaginary part is zero');
    checkNear(0.0, $orbit['mag'][0], 0.0, 'index 0 magnitude is zero');
    checkNear((float) $view->centerRe(), $orbit['zr'][1], 1e-16, 'index 1 is c0');
    checkNear((float) $view->centerIm(), $orbit['zi'][1], 1e-16, 'index 1 is c0');

    [$expectedZr, $expectedZi] = bcmathOrbit($view->centerRe(), $view->centerIm(), $view->precision(), $limit);
    $worst = 0.0;
    foreach ([0, 1, 2, 3, 5, 10, 50, 100, 500, 1000, $limit] as $n) {
        $worst = max($worst, abs($orbit['zr'][$n] - $expectedZr[$n]), abs($orbit['zi'][$n] - $expectedZi[$n]));
    }
    check($worst <= 1e-15, "double projection matches bcmath at sampled indices (worst {$worst})");
    $magWorst = 0.0;
    foreach ([1, 2, 10, 100, 1000, $limit] as $n) {
        $magWorst = max($magWorst, abs($orbit['mag'][$n] - ($orbit['zr'][$n] ** 2 + $orbit['zi'][$n] ** 2)));
    }
    check($magWorst <= 1e-12, "magnitude is the squared modulus (worst {$magWorst})");

    $candidates = $reference->highPrecision();
    checkSame(Reference::CANDIDATE_COUNT, count($candidates), 'candidate count');
    checkSame(['index', 're', 'im', 'mag'], array_keys($candidates[0]), 'candidate keys');
    checkSame(0, $candidates[0]['index'], 'index 0 is the smallest magnitude');
    checkNear(0.0, $candidates[0]['mag'], 0.0, 'index 0 magnitude');
    $sorted = true;
    $smallest = INF;
    foreach ($orbit['mag'] as $magnitude) {
        if ($magnitude < $smallest) {
            $smallest = $magnitude;
        }
    }
    for ($i = 1; $i < count($candidates); $i++) {
        if ($candidates[$i]['mag'] < $candidates[$i - 1]['mag']) {
            $sorted = false;
        }
    }
    check($sorted, 'candidates are sorted by magnitude');
    checkNear($smallest, $candidates[0]['mag'], 1e-15, 'candidate 0 is the smallest |Z| of the orbit');
    check(is_numeric($candidates[0]['re']) && is_numeric($candidates[0]['im']), 'candidates carry decimal strings');

    checkSame(2, $reference->build(), 'second build bumps the version');

    $scale = $view->scale();
    $precision = $view->precision();
    $same = 0;
    $tested = 0;
    for ($y = 0; $y < 4; $y++) {
        for ($x = 0; $x < 5; $x++) {
            [$cRe, $cIm] = pixelCenter($view, $x * 7, $y * 5, 1, $precision, $scale);
            [$count, $magnitude] = bcmathPixel($cRe, $cIm, $precision, $limit);
            $expected = pixelColor($count, $magnitude, $limit);
            if ($reference->renderPixelSlow($x * 7, $y * 5, 1, $limit) === $expected) {
                $same++;
            }
            $tested++;
        }
    }
    checkSame($tested, $same, 'renderPixelSlow matches an independent bcmath iteration');
}
