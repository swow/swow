#!/usr/bin/env php
<?php
declare(strict_types=1);

/*
* Standalone GUI smoke test for the Cocoa backend.
*
* Needs macOS with a logged-in console session and a PHP build with FFI:
*   /opt/homebrew/bin/php tests/gui/cocoa-smoke.php
*
* The test does not load the swow extension. It snapshots the content view
* through AppKit because screencapture from an ssh session is subject to the
* Screen Recording permission and may omit every window.
*/

if (PHP_OS_FAMILY !== 'Darwin') {
    fwrite(STDERR, "this test needs macOS\n");
    exit(1);
}

$root = dirname(__DIR__, 2);
require_once $root . '/src/Gui/Backend.php';
require_once $root . '/src/Gui/Layout.php';
require_once $root . '/src/Gui/Cocoa/ObjC.php';
require_once $root . '/src/Gui/Cocoa.php';

use Swow\Mandelbrot\Gui\Cocoa;
use Swow\Mandelbrot\Gui\Layout;

const WINDOW_WIDTH = 1280;
const WINDOW_HEIGHT = 720;
const IMAGE_WIDTH = WINDOW_WIDTH;
const IMAGE_HEIGHT = WINDOW_HEIGHT - Layout::TOOLBAR_HEIGHT;

function fail(string $message): never
{
    fwrite(STDERR, 'FAIL: ' . $message . "\n");
    exit(1);
}

function check(bool $condition, string $message): void
{
    if (!$condition) {
        fail($message);
    }
    echo 'ok - ', $message, "\n";
    fflush(STDOUT);
}

/**
 * Build a tightly packed BGRA gradient.
 *
 * The full and preview images use different red ramps so the two snapshots
 * differ. Alpha stays 255, as the shared buffer guarantees.
 */
function gradientBytes(int $width, int $height, bool $preview): string
{
    $rows = [];
    for ($y = 0; $y < $height; $y++) {
        $green = intdiv($y * 255, max(1, $height - 1));
        $row = '';
        for ($x = 0; $x < $width; $x++) {
            $blue = intdiv($x * 255, max(1, $width - 1));
            $red = $preview
                ? 255 - $blue
                : intdiv(($x + $y) * 255, max(1, $width + $height - 2));
            $row .= pack('C4', $blue, $green, $red, 255);
        }
        $rows[] = $row;
    }

    return implode('', $rows);
}

function pumpFor(Cocoa $cocoa, float $seconds): void
{
    $deadline = microtime(true) + $seconds;
    while (microtime(true) < $deadline) {
        $events = $cocoa->pump();
        if ($events !== []) {
            fail('the smoke test must not emit events, got ' . json_encode($events));
        }
        usleep(10_000);
    }
}

function snapshot(Cocoa $cocoa, string $path): void
{
    check($cocoa->snapshot($path), 'snapshot written to ' . $path);
    $info = getimagesize($path);
    if ($info === false) {
        fail('not a readable image: ' . $path);
    }
    check($info[2] === IMAGETYPE_PNG, 'snapshot is a PNG');
    check($info[0] >= WINDOW_WIDTH && $info[1] >= WINDOW_HEIGHT, 'snapshot covers the window');
}

/**
 * Read one pixel at logical window coordinates.
 *
 * The bitmap rep follows the backing scale, so coordinates are multiplied by
 * the ratio between the snapshot and the 1280x720 client size.
 *
 * @return array{0: int, 1: int, 2: int}
 */
function pixelAt(string $path, int $x, int $y): array
{
    $image = imagecreatefrompng($path);
    if ($image === false) {
        fail('cannot decode PNG: ' . $path);
    }
    $scale = max(1, intdiv(imagesx($image), WINDOW_WIDTH));
    $color = imagecolorat($image, $x * $scale, $y * $scale);
    imagedestroy($image);

    return [($color >> 16) & 0xff, ($color >> 8) & 0xff, $color & 0xff];
}

$outDir = __DIR__ . '/out';
if (!is_dir($outDir) && !mkdir($outDir, 0o777, true) && !is_dir($outDir)) {
    fail('cannot create output directory ' . $outDir);
}

$rects = Layout::resolve(Layout::tree(), Layout::metrics(), WINDOW_WIDTH, WINDOW_HEIGHT);

$cocoa = new Cocoa();
$cocoa->open(WINDOW_WIDTH, WINDOW_HEIGHT);
pumpFor($cocoa, 0.2);

$geometry = $cocoa->windowGeometry();
check($geometry['title'] === Layout::TITLE, 'window title is ' . Layout::TITLE);
$cocoa->setTitle('Swow Mandelbrot Auto Zoom');
check(
    $cocoa->windowGeometry()['title'] === 'Swow Mandelbrot Auto Zoom',
    'setTitle updates the window title',
);
$cocoa->setTitle(Layout::TITLE);
check($cocoa->windowGeometry()['title'] === Layout::TITLE, 'setTitle restores the initial title');
check($geometry['width'] === WINDOW_WIDTH, 'client width is ' . WINDOW_WIDTH);
check($geometry['height'] === WINDOW_HEIGHT, 'client height is ' . WINDOW_HEIGHT);
check(!$geometry['resizable'], 'window is not resizable');
check($geometry['visible'], 'window is visible');
check($cocoa->imageWidth() === IMAGE_WIDTH, 'imageWidth() is ' . IMAGE_WIDTH);
check($cocoa->imageHeight() === IMAGE_HEIGHT, 'imageHeight() is ' . IMAGE_HEIGHT);

$placed = [
    Layout::COMP_REAL,
    Layout::COMP_IMAG,
    Layout::COMP_DEPTH,
    Layout::COMP_ZOOM_OUT,
    Layout::COMP_ZOOM_IN,
    Layout::COMP_RESET,
    Layout::COMP_GO,
    Layout::COMP_SAVE,
    Layout::COMP_QUIT,
    Layout::COMP_PAN_UP,
    Layout::COMP_PAN_DOWN,
    Layout::COMP_PAN_LEFT,
    Layout::COMP_PAN_RIGHT,
    Layout::COMP_STATUS,
];
foreach ($placed as $component) {
    $rect = $rects[$component] ?? fail('layout has no rect for ' . $component);
    $actual = $cocoa->widgetFrame($component);
    check(
        $actual === ['x' => $rect->x, 'y' => $rect->y, 'w' => $rect->w, 'h' => $rect->h],
        $component . ' is placed at the layout rect '
            . sprintf('%d,%d %dx%d', $rect->x, $rect->y, $rect->w, $rect->h),
    );
}

$wiring = [
    Layout::COMP_ZOOM_OUT => 'zoomOut:',
    Layout::COMP_ZOOM_IN => 'zoomIn:',
    Layout::COMP_RESET => 'reset:',
    Layout::COMP_GO => 'go:',
    Layout::COMP_SAVE => 'save:',
    Layout::COMP_QUIT => 'quit:',
    Layout::COMP_PAN_UP => 'panUp:',
    Layout::COMP_PAN_DOWN => 'panDown:',
    Layout::COMP_PAN_LEFT => 'panLeft:',
    Layout::COMP_PAN_RIGHT => 'panRight:',
    Layout::COMP_REAL => 'go:',
    Layout::COMP_IMAG => 'go:',
    Layout::COMP_DEPTH => 'go:',
];
foreach ($wiring as $component => $action) {
    check(
        $cocoa->widgetAction($component) === $action && $cocoa->widgetIsWired($component),
        $component . ' sends ' . $action . ' to the PHP controller',
    );
}

check($cocoa->placeholderShown(), 'placeholder is shown before the first frame');
$placeholderPath = $outDir . '/cocoa-smoke-placeholder.png';
snapshot($cocoa, $placeholderPath);

$cocoa->setFields('-0.743643887037151', '0.13182590420533', '1e4');
check($cocoa->fieldValue(Layout::COMP_REAL) === '-0.743643887037151', 'setFields fills the real field');
check($cocoa->fieldValue(Layout::COMP_IMAG) === '0.13182590420533', 'setFields fills the imaginary field');
check($cocoa->fieldValue(Layout::COMP_DEPTH) === '1e4', 'setFields fills the depth field');

$defaultBackgrounds = [
    Layout::COMP_REAL => $cocoa->fieldBackground(Layout::COMP_REAL),
    Layout::COMP_IMAG => $cocoa->fieldBackground(Layout::COMP_IMAG),
    Layout::COMP_DEPTH => $cocoa->fieldBackground(Layout::COMP_DEPTH),
];
$cocoa->markInvalid('depth', 'invalid depth');
$depthBackground = $cocoa->fieldBackground(Layout::COMP_DEPTH);
check(
    $depthBackground[0] > 0.99 && $depthBackground[1] < 0.01 && $depthBackground[2] < 0.01,
    'markInvalid paints the depth field red',
);
check(
    $cocoa->fieldBackground(Layout::COMP_REAL) === $defaultBackgrounds[Layout::COMP_REAL]
        && $cocoa->fieldBackground(Layout::COMP_IMAG) === $defaultBackgrounds[Layout::COMP_IMAG],
    'markInvalid leaves the other fields alone',
);
check($cocoa->statusValue() === 'invalid depth', 'markInvalid writes the reason to the status line');
$cocoa->markInvalid(null, '');
check(
    [
        Layout::COMP_REAL => $cocoa->fieldBackground(Layout::COMP_REAL),
        Layout::COMP_IMAG => $cocoa->fieldBackground(Layout::COMP_IMAG),
        Layout::COMP_DEPTH => $cocoa->fieldBackground(Layout::COMP_DEPTH),
    ] === $defaultBackgrounds,
    'markInvalid(null) restores the field backgrounds',
);
check($cocoa->statusValue() === '', 'markInvalid(null) clears the status line');

try {
    $cocoa->markInvalid('center', 'x');
    fail('unknown invalid field was accepted');
} catch (InvalidArgumentException) {
    echo "ok - unknown invalid field rejected\n";
}

$cocoa->setBusy(true);
foreach ([
    Layout::COMP_REAL,
    Layout::COMP_IMAG,
    Layout::COMP_DEPTH,
    Layout::COMP_ZOOM_OUT,
    Layout::COMP_ZOOM_IN,
    Layout::COMP_RESET,
    Layout::COMP_GO,
    Layout::COMP_SAVE,
    Layout::COMP_PAN_UP,
    Layout::COMP_PAN_DOWN,
    Layout::COMP_PAN_LEFT,
    Layout::COMP_PAN_RIGHT,
] as $component) {
    check(!$cocoa->widgetEnabled($component), $component . ' is disabled while busy');
}
check($cocoa->widgetEnabled(Layout::COMP_QUIT), 'Quit stays enabled while busy');
$cocoa->setBusy(true);
check(!$cocoa->widgetEnabled(Layout::COMP_GO), 'a second setBusy(true) keeps the fields disabled');
$cocoa->setBusy(false);
foreach ([
    Layout::COMP_REAL,
    Layout::COMP_IMAG,
    Layout::COMP_DEPTH,
    Layout::COMP_ZOOM_OUT,
    Layout::COMP_ZOOM_IN,
    Layout::COMP_RESET,
    Layout::COMP_GO,
    Layout::COMP_SAVE,
    Layout::COMP_PAN_UP,
    Layout::COMP_PAN_DOWN,
    Layout::COMP_PAN_LEFT,
    Layout::COMP_PAN_RIGHT,
] as $component) {
    check($cocoa->widgetEnabled($component), $component . ' is enabled after setBusy(false)');
}
check($cocoa->widgetEnabled(Layout::COMP_QUIT), 'Quit stays enabled after setBusy(false)');

try {
    $cocoa->presentRows('short', 0, 1, IMAGE_WIDTH, false);
    fail('short pixel payload was accepted');
} catch (InvalidArgumentException) {
    echo "ok - short pixel payload rejected\n";
}
check(!$cocoa->placeholderShown(), 'placeholder is gone after the first presentRows');

$statusText = 'status {braces} "quotes" $dollar [brackets] \\slash';
$cocoa->status($statusText);
check($cocoa->statusValue() === $statusText, 'status text survives NSString conversion');

$previewWidth = intdiv(IMAGE_WIDTH, 4);
$previewHeight = intdiv(IMAGE_HEIGHT, 4);
$cocoa->setImageMode(true);
check($cocoa->isPreviewMode(), 'setImageMode(true) selects the preview');
$cocoa->presentRows(
    gradientBytes($previewWidth, 1, true),
    0,
    1,
    $previewWidth,
    true,
);
check(
    $cocoa->imageSize() === [$previewWidth, $previewHeight],
    'preview image is ' . $previewWidth . 'x' . $previewHeight,
);
$previewBytes = gradientBytes($previewWidth, $previewHeight, true);
for ($rowBegin = 0; $rowBegin < $previewHeight; $rowBegin += 50) {
    $rowEnd = min($rowBegin + 50, $previewHeight);
    $cocoa->presentRows(
        substr($previewBytes, $rowBegin * $previewWidth * 4, ($rowEnd - $rowBegin) * $previewWidth * 4),
        $rowBegin,
        $rowEnd,
        $previewWidth,
        true,
    );
}
$cocoa->status('smoke test: preview bands submitted');
pumpFor($cocoa, 0.25);

$previewPath = $outDir . '/cocoa-smoke-preview.png';
snapshot($cocoa, $previewPath);
check(pixelAt($previewPath, 0, Layout::TOOLBAR_HEIGHT) === [255, 0, 0], 'preview top left pixel is red');
check(pixelAt($previewPath, 400, Layout::TOOLBAR_HEIGHT) === [176, 0, 79], 'preview is zoomed 4x horizontally');
check(pixelAt($previewPath, 0, Layout::TOOLBAR_HEIGHT - 1) === [255, 255, 255], 'toolbar strip stays above the image');
check(
    pixelAt($previewPath, IMAGE_WIDTH - 1, WINDOW_HEIGHT - 1) === [0, 255, 255],
    'preview bottom right pixel matches the gradient',
);

$fullBytes = gradientBytes(IMAGE_WIDTH, IMAGE_HEIGHT, false);
for ($rowBegin = 0; $rowBegin < IMAGE_HEIGHT; $rowBegin += 64) {
    $rowEnd = min($rowBegin + 64, IMAGE_HEIGHT);
    $cocoa->presentRows(
        substr($fullBytes, $rowBegin * IMAGE_WIDTH * 4, ($rowEnd - $rowBegin) * IMAGE_WIDTH * 4),
        $rowBegin,
        $rowEnd,
        IMAGE_WIDTH,
        false,
    );
}
$cocoa->status('smoke test: full bands submitted');
pumpFor($cocoa, 0.25);
check(
    $cocoa->imageSize() === [$previewWidth, $previewHeight],
    'preview stays selected while full bands arrive',
);

$cocoa->setImageMode(false);
check(!$cocoa->isPreviewMode(), 'setImageMode(false) selects the full image');
check($cocoa->imageSize() === [IMAGE_WIDTH, IMAGE_HEIGHT], 'full image is ' . IMAGE_WIDTH . 'x' . IMAGE_HEIGHT);
pumpFor($cocoa, 0.1);

$fullPath = $outDir . '/cocoa-smoke-full.png';
snapshot($cocoa, $fullPath);
check(pixelAt($fullPath, 0, Layout::TOOLBAR_HEIGHT) === [0, 0, 0], 'full image top left pixel is black');
check(
    pixelAt($fullPath, IMAGE_WIDTH - 1, WINDOW_HEIGHT - 1) === [255, 255, 255],
    'full image bottom right pixel is white',
);
check(pixelAt($fullPath, 640, Layout::TOOLBAR_HEIGHT + 338) === [127, 127, 127], 'full image middle pixel matches the gradient');

$cocoa->setImageMode(true);
check($cocoa->isPreviewMode() && $cocoa->imageSize() === [$previewWidth, $previewHeight], 'setImageMode(true) shows the preview again');
$cocoa->setImageMode(false);
check(!$cocoa->isPreviewMode() && $cocoa->imageSize() === [IMAGE_WIDTH, IMAGE_HEIGHT], 'setImageMode(false) shows the full image again');

$cocoa->close();
echo "PASS: Cocoa smoke test\n";
echo 'placeholder PNG: ', $placeholderPath, "\n";
echo 'preview PNG: ', $previewPath, "\n";
echo 'full PNG: ', $fullPath, "\n";
