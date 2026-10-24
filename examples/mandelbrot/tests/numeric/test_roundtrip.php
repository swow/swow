<?php

declare(strict_types=1);

use Swow\Mandelbrot\Reference;
use Swow\Mandelbrot\Renderer;
use Swow\Mandelbrot\View;

/**
 * View format and re-parse round trip.
 *
 * The GUI writes View::format() into the entries and reads them back on Go, so
 * the formatted fields must carry every decimal place of the stored centre and
 * parsing them must give the same strings. A frame rendered before and after
 * the round trip must be byte identical.
 */
function testRoundTrip(): void
{
    echo "Round trip\n";

    // format -> setCenter/setDepth -> format is stable at every depth, for the
    // default centre and for a centre with more decimals than the depth needs
    foreach (['1', '1e4', '1e14', '1e30'] as $depth) {
        foreach (['default', 'deep'] as $kind) {
            $view = new View(128, 72);
            if ($kind === 'deep') {
                $view->setCenter(DEEP_CENTER_RE, DEEP_CENTER_IM);
            }
            $view->setDepth($depth);
            $formatted = $view->format();
            $view->setCenter($formatted['re'], $formatted['im']);
            $view->setDepth($formatted['depth']);
            checkSame($formatted, $view->format(), "format is stable at depth {$depth} with the {$kind} centre");
        }
    }

    // the formatted centre keeps the decimals it was given
    $view = new View(128, 72);
    $view->setDepth('1e4');
    checkSame('-0.743643887018689113498922249712800000007037228233600378464531', $view->format()['re'], 'the default centre keeps all 60 decimals at 1e4');
    $view->setCenter(DEEP_CENTER_RE, DEEP_CENTER_IM);
    checkSame(DEEP_CENTER_RE, $view->format()['re'], 'a 60 digit centre keeps all 60 decimals at 1e4');
    $view->setDepth('1e14');
    checkSame(DEEP_CENTER_RE, $view->format()['re'], 'a 60 digit centre keeps all 60 decimals at 1e14');
    $view->setDepth('1e30');
    checkSame(DEEP_CENTER_RE, $view->format()['re'], 'a 60 digit centre keeps all 60 decimals at 1e30');

    // format of a parsed value is the canonical form of the input text
    $centres = [
        [View::DEFAULT_CENTER_RE, View::DEFAULT_CENTER_IM],
        [DEEP_CENTER_RE, DEEP_CENTER_IM],
        ['-0.74364388', '0'],
        ['-0.000000000000000000001', '0.5'],
        ['+0.5000', '007.250'],
    ];
    foreach ($centres as [$re, $im]) {
        $view = new View(128, 72);
        $view->setCenter($re, $im);
        $formatted = $view->format();
        checkSame($view->centerRe(), $formatted['re'], "centre text round trip for {$re}");
        checkSame($view->centerIm(), $formatted['im'], "centre text round trip for {$im}");
    }
    $view = new View(128, 72);
    $view->setCenter('+0.5000', '007.250');
    checkSame('0.5', $view->centerRe(), 'canonical centre drops the plus and trailing zeros');
    checkSame('7.25', $view->centerIm(), 'canonical centre drops leading integer zeros');
    $view->setCenter('-0.000000000000000000001', '0');
    checkSame('-0.000000000000000000001', $view->centerRe(), 'canonical centre keeps tiny values');
    checkSame('0', $view->centerIm(), 'canonical centre turns -0 into 0');

    $depths = [
        ['1e14', '100000000000000'],
        ['1.234567890123456789e14', '123456789012345.6789'],
        ['2', '2'],
        ['0.5', '0.5'],
    ];
    foreach ($depths as [$input, $canonical]) {
        $view = new View(128, 72);
        $view->setDepth($input);
        checkSame($canonical, $view->format()['depth'], "depth {$input} canonicalises to {$canonical}");
        $view->setDepth($view->format()['depth']);
        checkSame($canonical, $view->depthText(), "depth {$input} survives the round trip");
    }

    // 40 zoom steps each way are exact in decimal and restore the same text
    foreach (['10000', '2', '0.5', '123456789012345.6789'] as $start) {
        $view = new View(128, 72);
        $view->setDepth($start);
        for ($i = 0; $i < 40; $i++) {
            $view->zoom(2.0);
        }
        for ($i = 0; $i < 40; $i++) {
            $view->zoom(0.5);
        }
        checkSame($start, $view->depthText(), "40 zoom in and out from {$start} restore the text");
    }

    // pan by a quarter of the image and back restores the exact centre text
    $panCentres = [
        [View::DEFAULT_CENTER_RE, View::DEFAULT_CENTER_IM],
        [DEEP_CENTER_RE, DEEP_CENTER_IM],
    ];
    foreach (['1e4', '1e14', '1e30'] as $depth) {
        foreach ($panCentres as [$re, $im]) {
            $view = new View(640, 360);
            $view->setCenter($re, $im);
            $view->setDepth($depth);
            $beforeRe = $view->centerRe();
            $beforeIm = $view->centerIm();
            $view->pan(160.0, 0.0, 640, 360);
            check($view->centerRe() !== $beforeRe, "pan x moves the centre at depth {$depth}");
            $view->pan(-160.0, 0.0, 640, 360);
            checkSame($beforeRe, $view->centerRe(), "pan x round trip at depth {$depth}");
            checkSame($beforeIm, $view->centerIm(), "pan x leaves the imaginary part at depth {$depth}");
            $view->pan(0.0, 90.0, 640, 360);
            check($view->centerIm() !== $beforeIm, "pan y moves the centre at depth {$depth}");
            $view->pan(0.0, -90.0, 640, 360);
            checkSame($beforeIm, $view->centerIm(), "pan y round trip at depth {$depth}");
            checkSame($beforeRe, $view->centerRe(), "pan y leaves the real part at depth {$depth}");
            $view->pan(0.0, 0.0, 640, 360);
            checkSame($beforeRe, $view->centerRe(), "a zero pan changes nothing at depth {$depth}");
        }
    }

    // one pixel at 1e14 is 18 / (5 * height * depth) = 1e-16 units for a 360
    // pixel high image; the centre changes in its deep digits and keeps them
    $view = new View(640, 360);
    $view->setCenter(DEEP_CENTER_RE, DEEP_CENTER_IM);
    $view->setDepth('1e14');
    $beforeRe = $view->centerRe();
    $view->pan(1.0, 0.0, 640, 360);
    $afterRe = $view->centerRe();
    $decimals = strpos($afterRe, '.') === false ? 0 : strlen($afterRe) - strpos($afterRe, '.') - 1;
    check($afterRe !== $beforeRe, 'a one pixel pan changes the centre');
    checkSame(60, $decimals, 'a one pixel pan keeps all 60 stored decimals');
    checkSame('0.0000000000000001', bcsub($beforeRe, $afterRe, 60) === '0.000000000000000100000000000000000000000000000000000000000000'
        ? '0.0000000000000001' : bcsub($beforeRe, $afterRe, 60), 'a one pixel pan moves the real part by 1e-16');
    $view->pan(-1.0, 0.0, 640, 360);
    checkSame($beforeRe, $view->centerRe(), 'a one pixel pan round trips exactly');
    // panning by 0.5 pixel is also exact text
    $view->pan(0.5, 0.0, 640, 360);
    $view->pan(-0.5, 0.0, 640, 360);
    checkSame($beforeRe, $view->centerRe(), 'a half pixel pan round trips exactly');

    // a rendered frame survives the round trip byte for byte
    foreach ([['1e4', 'default'], ['1e14', 'deep']] as [$depth, $kind]) {
        $view = new View(128, 72);
        if ($kind === 'deep') {
            $view->setCenter(DEEP_CENTER_RE, DEEP_CENTER_IM);
        }
        $view->setDepth($depth);
        $reference = new Reference($view);
        $reference->build();
        $params = $view->params(128, 72, 1);
        $first = Renderer::renderBand($reference->orbit(), $params, 0, 72);
        $formatted = $view->format();
        $view->setCenter($formatted['re'], $formatted['im']);
        $view->setDepth($formatted['depth']);
        $reference = new Reference($view);
        $reference->build();
        $paramsAfter = $view->params(128, 72, 1);
        $second = Renderer::renderBand($reference->orbit(), $paramsAfter, 0, 72);
        checkSame($formatted, $view->format(), "the {$depth} {$kind} view is unchanged after the round trip");
        checkSame($params, $paramsAfter, "the {$depth} {$kind} params are unchanged after the round trip");
        checkSame(
            digestColors($first['colors']),
            digestColors($second['colors']),
            "128x72 {$depth} {$kind} renders identically after the round trip"
        );
        printf("  128x72 %s %s digest %s\n", $depth, $kind, digestColors($first['colors']));
    }
}
