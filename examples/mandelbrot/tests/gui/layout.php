#!/usr/bin/env php
<?php

declare(strict_types=1);

/*
* Pure layout test: no Tk, no display.
*
* Resolves the platform-neutral tree for the fixed 1280x720 window and checks
* the toolbar geometry, the canvas rect and the absence of overlaps.
*/

require_once dirname(__DIR__, 2) . '/src/Gui/Layout.php';

use Swow\Mandelbrot\Gui\Layout;
use Swow\Mandelbrot\Gui\Rect;

const WINDOW_WIDTH = 1280;
const WINDOW_HEIGHT = 720;

function fail(string $message): never
{
    fwrite(STDERR, 'FAIL: ' . $message . "\n");
    exit(1);
}

function check(bool $condition, string $message): void
{
    if ($condition) {
        echo 'ok - ', $message, "\n";

        return;
    }
    fail($message);
}

function overlaps(Rect $a, Rect $b): bool
{
    return $a->x < $b->x + $b->w &&
        $b->x < $a->x + $a->w &&
        $a->y < $b->y + $b->h &&
        $b->y < $a->y + $a->h;
}

$metrics = Layout::metrics();
$tree = Layout::tree();
$rects = Layout::resolve($tree, $metrics, WINDOW_WIDTH, WINDOW_HEIGHT);
$toolbarHeight = Layout::toolbarHeight($metrics);

check($toolbarHeight === Layout::TOOLBAR_HEIGHT, 'toolbar height is ' . Layout::TOOLBAR_HEIGHT);
check(
    $toolbarHeight === 44,
    'toolbar height is 44 DIP',
);

foreach (Layout::COMPONENTS as $component) {
    check(isset($rects[$component]), $component . ' has a resolved rect');
}

$canvas = $rects[Layout::COMP_CANVAS];
check(
    $canvas->x === 0 &&
        $canvas->y === $toolbarHeight &&
        $canvas->w === WINDOW_WIDTH &&
        $canvas->h === WINDOW_HEIGHT - $toolbarHeight,
    sprintf(
        'canvas rect is 0,%d %dx%d',
        $toolbarHeight,
        WINDOW_WIDTH,
        WINDOW_HEIGHT - $toolbarHeight,
    ),
);
check(
    $canvas->h === WINDOW_HEIGHT - 44,
    'canvas height is the image area height 676',
);

$window = new Rect(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);
foreach ($rects as $key => $rect) {
    check($rect->w > 0 && $rect->h > 0, $key . ' has a positive rect');
    check(
        $rect->x >= $window->x &&
            $rect->y >= $window->y &&
            $rect->x + $rect->w <= $window->x + $window->w &&
            $rect->y + $rect->h <= $window->y + $window->h,
        $key . ' is inside the window',
    );
    if ($key !== Layout::COMP_CANVAS) {
        check(
            $rect->y >= 0 && $rect->y + $rect->h <= $toolbarHeight,
            $key . ' is inside the toolbar',
        );
    }
}

$keys = array_keys($rects);
$count = count($keys);
for ($i = 0; $i < $count; $i++) {
    for ($j = $i + 1; $j < $count; $j++) {
        check(
            !overlaps($rects[$keys[$i]], $rects[$keys[$j]]),
            $keys[$i] . ' does not overlap ' . $keys[$j],
        );
    }
}

$realLabel = $rects['label:Real'];
check(
    $realLabel->x === 2 && $realLabel->y === 1 && $realLabel->w === 33 && $realLabel->h === 25,
    'Real label keeps the previous geometry',
);
check(
    $rects[Layout::COMP_REAL]->x === 39 &&
        $rects[Layout::COMP_REAL]->y === 0 &&
        $rects[Layout::COMP_REAL]->w === 246 &&
        $rects[Layout::COMP_REAL]->h === 27,
    'Real entry keeps the previous geometry',
);
check(
    $rects[Layout::COMP_QUIT]->x === 1006 &&
        $rects[Layout::COMP_QUIT]->w === 58 &&
        $rects[Layout::COMP_QUIT]->h === 27,
    'Quit button keeps the previous geometry',
);
$panRects = [
    Layout::COMP_PAN_UP => 1068,
    Layout::COMP_PAN_DOWN => 1100,
    Layout::COMP_PAN_LEFT => 1132,
    Layout::COMP_PAN_RIGHT => 1164,
];
foreach ($panRects as $component => $x) {
    $rect = $rects[$component];
    check(
        $rect->x === $x && $rect->y === 0 && $rect->w === 28 && $rect->h === 27,
        $component . ' is the compact pan button at ' . $x . ',0 28x27',
    );
}
$status = $rects[Layout::COMP_STATUS];
check(
    $status->x === 2 && $status->y === 28 && $status->w === 1276 && $status->h === 16,
    'status label sits at the toolbar bottom',
);

echo "PASS: layout test\n";
