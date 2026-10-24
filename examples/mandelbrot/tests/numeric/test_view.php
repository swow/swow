<?php

declare(strict_types=1);

use Swow\Mandelbrot\View;

function testView(): void
{
    echo "View\n";
    $view = new View(1280, 676);
    checkSame('-0.743643887018689113498922249712800000007037228233600378464531', $view->centerRe(), 'default real centre');
    checkSame('0.131825904211894657975878336475200000004669779776843260798656', $view->centerIm(), 'default imaginary centre');
    checkNear(1.0e4, $view->depth(), 0.0, 'default depth');
    checkNear(300.0 * (676 / 1080) * 1.0e4, $view->scale(), 1e-9, 'scale is 300 * height/1080 * depth');
    checkSame(1874, $view->limit(), 'limit 1024 + 64*log2(depth) at 1e4');
    checkSame(8, $view->digits(), 'display digits are depth digits + 3 at 1e4');
    checkSame(25, $view->precision(), 'bcmath digits are depth digits + 20 at 1e4');
    checkSame(1280, $view->width(), 'width');
    checkSame(676, $view->height(), 'height');

    $view->setDepth('1e10');
    checkSame(3150, $view->limit(), 'limit at 1e10');
    checkSame(14, $view->digits(), 'digits at 1e10');
    checkSame(31, $view->precision(), 'precision at 1e10');
    $view->setDepth('1e20');
    checkSame(5276, $view->limit(), 'limit at 1e20');
    checkSame(24, $view->digits(), 'digits at 1e20');
    checkSame(41, $view->precision(), 'precision at 1e20');
    $view->setDepth('1e30');
    checkSame(7402, $view->limit(), 'limit at 1e30');
    checkSame(34, $view->digits(), 'digits at 1e30');
    checkSame(51, $view->precision(), 'precision at 1e30');

    $base = new View(100, 100, 2000);
    checkSame(2000 + (int) (64.0 * log(1.0e4) / log(2.0)), $base->limit(), 'base iterations are honoured');

    $view = new View(1280, 676);
    $view->setDepth('3e-1');
    checkNear(0.3, $view->depth(), 1e-15, 'depth accepts plain exponent text');
    checkSame('0.3', $view->depthText(), 'depth text is canonical plain decimal');
    $view->setDepth(' 10000 ');
    checkNear(1.0e4, $view->depth(), 0.0, 'depth is trimmed');
    checkSame('10000', $view->depthText(), 'depth text keeps plain integers');
    $view->setDepth('1.234567890123456789e14');
    checkSame('123456789012345.6789', $view->depthText(), 'a long depth keeps every digit');
    checkNear(1.234567890123456789e14, $view->depthFloat(), 1e4, 'depthFloat reads the text');

    foreach (['0', '-1', 'abc', '', '1e400', 'inf', 'nan', '0x10', '1e', '--1'] as $bad) {
        $thrown = false;
        try {
            $view->setDepth($bad);
        } catch (InvalidArgumentException) {
            $thrown = true;
        }
        check($thrown, "setDepth rejects '{$bad}'");
    }

    $view = new View(1280, 676);
    $view->setCenter('-0.5', '0.25');
    checkSame('-0.5', $view->centerRe(), 'setCenter keeps decimal text');
    checkSame('0.25', $view->centerIm(), 'setCenter keeps decimal text');
    $view->setCenter('1e-3', '-0');
    checkSame('0.001', $view->centerRe(), 'setCenter expands exponent notation');
    checkSame('0', $view->centerIm(), 'setCenter normalises negative zero');
    $view->setCenter(' 0.5 ', '0.5');
    checkSame('0.5', $view->centerRe(), 'setCenter trims');
    foreach ([['x', '0'], ['0', 'y'], ['', '0'], ['1e999', '0']] as [$re, $im]) {
        $thrown = false;
        try {
            $view->setCenter($re, $im);
        } catch (InvalidArgumentException) {
            $thrown = true;
        }
        check($thrown, "setCenter rejects ('{$re}', '{$im}')");
    }

    $view = new View(1280, 676);
    $view->setCenter('-0.1', '0.2');
    $view->setDepth('1e4');
    $view->zoom(2.0);
    checkNear(2.0e4, $view->depth(), 0.0, 'zoom multiplies depth');
    checkSame('20000', $view->depthText(), 'zoom text is exact');
    checkSame('-0.1', $view->centerRe(), 'zoom keeps the real centre');
    checkSame('0.2', $view->centerIm(), 'zoom keeps the imaginary centre');
    $view->zoom(0.5);
    checkNear(1.0e4, $view->depth(), 0.0, 'zoom back');
    checkSame('10000', $view->depthText(), 'zoom in and out restore the exact text');
    foreach ([0.0, -1.0, INF, NAN] as $bad) {
        $thrown = false;
        try {
            $view->zoom($bad);
        } catch (InvalidArgumentException) {
            $thrown = true;
        }
        check($thrown, 'zoom rejects a non-positive or non-finite factor');
    }

    $view->reset();
    checkNear(1.0e4, $view->depth(), 0.0, 'reset restores the depth');
    checkSame('-0.743643887018689113498922249712800000007037228233600378464531', $view->centerRe(), 'reset restores the centre');

    $view->setDepth('1e4');
    $format = $view->format();
    checkSame(['re', 'im', 'depth'], array_keys($format), 'format keys');
    checkSame('-0.743643887018689113498922249712800000007037228233600378464531', $format['re'], 'format keeps every stored decimal');
    checkSame('10000', $format['depth'], 'format prints the depth');

    $params = $view->params(1280, 676, 1);
    checkSame(
        ['centerRe', 'centerIm', 'scale', 'limit', 'fullWidth', 'fullHeight', 'width', 'height', 'step'],
        array_keys($params),
        'params keys'
    );
    checkSame(1280, $params['width'], 'params full width');
    checkSame(676, $params['height'], 'params full height');
    checkNear($view->scale(), $params['scale'], 0.0, 'params scale');
    checkSame($view->limit(), $params['limit'], 'params limit');
    $preview = $view->params(1280, 676, 4);
    checkSame(320, $preview['width'], 'preview width is a quarter');
    checkSame(169, $preview['height'], 'preview height is a quarter');
    checkSame(4, $preview['step'], 'preview step');

    $thrown = false;
    try {
        $view->params(0, 100, 1);
    } catch (InvalidArgumentException) {
        $thrown = true;
    }
    check($thrown, 'params rejects an empty image');

    foreach ([[0, 100], [100, 0]] as [$w, $h]) {
        $thrown = false;
        try {
            new View($w, $h);
        } catch (InvalidArgumentException) {
            $thrown = true;
        }
        check($thrown, "constructor rejects {$w}x{$h}");
    }
}
