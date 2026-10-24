#!/usr/bin/env php
<?php
declare(strict_types=1);

/*
* Standalone GUI smoke test for the Tk backend.
*
* Needs a running X server, for example:
*   Xvfb :99 -screen 0 1400x900x24 &
*   DISPLAY=:99 php tests/gui/tk-smoke.php
*
* The test does not load the swow extension.
*/

if ((getenv('DISPLAY') ?: '') === '') {
    fwrite(STDERR, "DISPLAY is not set; start Xvfb and export DISPLAY\n");
    exit(1);
}

$root = dirname(__DIR__, 2);
require_once $root . '/src/Gui/Backend.php';
require_once $root . '/src/Gui/Layout.php';
require_once $root . '/src/Gui/Tk.php';

use Swow\Mandelbrot\Gui\Layout;
use Swow\Mandelbrot\Gui\Tk;

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
}

/**
 * Build a tightly packed BGRA gradient.
 *
 * The full and preview images use different red ramps so their PNG files
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

function pumpFor(Tk $tk, float $seconds): void
{
    $deadline = microtime(true) + $seconds;
    while (microtime(true) < $deadline) {
        $events = $tk->pump();
        if ($events !== []) {
            fail('batch 1 must not emit events, got ' . json_encode($events));
        }
        usleep(10_000);
    }
}

function pngSize(string $path): array
{
    $info = getimagesize($path);
    if ($info === false) {
        fail('not a readable image: ' . $path);
    }

    return [$info[0], $info[1], $info[2]];
}

$outDir = __DIR__ . '/out';
if (!is_dir($outDir) && !mkdir($outDir, 0o777, true) && !is_dir($outDir)) {
    fail('cannot create output directory ' . $outDir);
}

$tk = new Tk();
$tk->open(WINDOW_WIDTH, WINDOW_HEIGHT);

pumpFor($tk, 0.1);
$geometry = $tk->evaluate('wm geometry .');
check(str_starts_with($geometry, WINDOW_WIDTH . 'x' . WINDOW_HEIGHT), 'window geometry is ' . $geometry);
check((int) $tk->evaluate('winfo width .') === WINDOW_WIDTH, 'client width is ' . WINDOW_WIDTH);
check((int) $tk->evaluate('winfo height .') === WINDOW_HEIGHT, 'client height is ' . WINDOW_HEIGHT);
check((int) $tk->evaluate('winfo ismapped .') === 1, 'window is mapped');
check((int) $tk->evaluate('winfo height .canvas') === IMAGE_HEIGHT, 'canvas height is ' . IMAGE_HEIGHT);
check((int) $tk->evaluate('winfo height .toolbar') === Layout::TOOLBAR_HEIGHT, 'toolbar height is ' . Layout::TOOLBAR_HEIGHT);
check($tk->evaluate('wm title .') === Layout::TITLE, 'window title is ' . Layout::TITLE);
$autoTitle = 'Swow Mandelbrot Auto Zoom';
$tk->setTitle($autoTitle);
check($tk->evaluate('wm title .') === $autoTitle, 'setTitle updates the window title');
$tk->setTitle(Layout::TITLE);
check($tk->evaluate('wm title .') === Layout::TITLE, 'setTitle restores the initial title');
check($tk->evaluate('wm resizable .') === '0 0', 'window is not resizable');
check(
    (int) $tk->evaluate('winfo y ' . Layout::STATUS) + (int) $tk->evaluate('winfo height ' . Layout::STATUS)
        === Layout::TOOLBAR_HEIGHT,
    'status label sits at the bottom of the toolbar',
);
check(
    $tk->evaluate('image width ' . Layout::FULL_IMAGE) === (string) IMAGE_WIDTH
        && $tk->evaluate('image height ' . Layout::FULL_IMAGE) === (string) IMAGE_HEIGHT,
    'full photo image is ' . IMAGE_WIDTH . 'x' . IMAGE_HEIGHT,
);
check($tk->imageWidth() === IMAGE_WIDTH, 'imageWidth() is ' . IMAGE_WIDTH);
check($tk->imageHeight() === IMAGE_HEIGHT, 'imageHeight() is ' . IMAGE_HEIGHT);

$layoutRects = Layout::resolve(Layout::tree(), Layout::metrics(), WINDOW_WIDTH, WINDOW_HEIGHT);
$panButtons = [
    Layout::COMP_PAN_UP => [Layout::BUTTON_PAN_UP, '↑'],
    Layout::COMP_PAN_DOWN => [Layout::BUTTON_PAN_DOWN, '↓'],
    Layout::COMP_PAN_LEFT => [Layout::BUTTON_PAN_LEFT, '←'],
    Layout::COMP_PAN_RIGHT => [Layout::BUTTON_PAN_RIGHT, '→'],
];
foreach ($panButtons as $component => [$widget, $label]) {
    $rect = $layoutRects[$component];
    check(
        (int) $tk->evaluate('winfo x ' . $widget) === $rect->x
            && (int) $tk->evaluate('winfo y ' . $widget) === $rect->y
            && (int) $tk->evaluate('winfo width ' . $widget) === $rect->w
            && (int) $tk->evaluate('winfo height ' . $widget) === $rect->h,
        $component . ' button is placed at '
            . sprintf('%d,%d %dx%d', $rect->x, $rect->y, $rect->w, $rect->h),
    );
    check($tk->evaluate($widget . ' cget -text') === $label, $component . ' label is ' . $label);
}

$defaultBackgrounds = [
    're' => $tk->evaluate(Layout::ENTRY_RE . ' cget -background'),
    'im' => $tk->evaluate(Layout::ENTRY_IM . ' cget -background'),
    'depth' => $tk->evaluate(Layout::ENTRY_DEPTH . ' cget -background'),
];
$tk->setFields('-0.743643887037151', '0.13182590420533', '1e4');
check($tk->evaluate(Layout::ENTRY_RE . ' get') === '-0.743643887037151', 'setFields fills the real entry');
check($tk->evaluate(Layout::ENTRY_IM . ' get') === '0.13182590420533', 'setFields fills the imaginary entry');
check($tk->evaluate(Layout::ENTRY_DEPTH . ' get') === '1e4', 'setFields fills the depth entry');

$tk->markInvalid('depth', 'invalid depth');
check(
    $tk->evaluate(Layout::ENTRY_DEPTH . ' cget -background') === '#ff6666',
    'markInvalid paints the depth entry red',
);
check(
    $tk->evaluate(Layout::ENTRY_RE . ' cget -background') === $defaultBackgrounds['re'],
    'markInvalid leaves other entries alone',
);
check(
    $tk->evaluate(Layout::STATUS . ' cget -text') === 'invalid depth',
    'markInvalid writes the reason to the status line',
);

$tk->markInvalid(null, '');
check(
    [
        're' => $tk->evaluate(Layout::ENTRY_RE . ' cget -background'),
        'im' => $tk->evaluate(Layout::ENTRY_IM . ' cget -background'),
        'depth' => $tk->evaluate(Layout::ENTRY_DEPTH . ' cget -background'),
    ] === $defaultBackgrounds,
    'markInvalid(null) restores the entry backgrounds',
);
check($tk->evaluate(Layout::STATUS . ' cget -text') === '', 'markInvalid(null) clears the status line');

try {
    $tk->markInvalid('center', 'x');
    fail('unknown invalid field was accepted');
} catch (InvalidArgumentException) {
    echo "ok - unknown invalid field rejected\n";
}

$placeholderItem = trim($tk->evaluate(Layout::CANVAS . ' find withtag ' . Layout::PLACEHOLDER_TAG));
check($placeholderItem !== '', 'placeholder item exists before the first frame');
check(
    $tk->evaluate(Layout::CANVAS . ' itemcget ' . $placeholderItem . ' -image') === Layout::PLACEHOLDER_IMAGE,
    'placeholder item uses the placeholder photo',
);
check(
    $tk->evaluate('image width ' . Layout::PLACEHOLDER_IMAGE) === (string) IMAGE_WIDTH
        && $tk->evaluate('image height ' . Layout::PLACEHOLDER_IMAGE) === (string) IMAGE_HEIGHT,
    'placeholder photo is ' . IMAGE_WIDTH . 'x' . IMAGE_HEIGHT,
);
$stack = preg_split('/\s+/', trim($tk->evaluate(Layout::CANVAS . ' find overlapping 0 0 1 1'))) ?: [];
check(end($stack) === $placeholderItem, 'placeholder is above the photo items');
check(
    (int) $tk->evaluate('winfo ismapped ' . Layout::CANVAS) === 1,
    'placeholder is mapped',
);

try {
    $tk->presentRows('short', 0, 1, IMAGE_WIDTH, false);
    fail('short pixel payload was accepted');
} catch (InvalidArgumentException) {
    echo "ok - short pixel payload rejected\n";
}
check(
    trim($tk->evaluate(Layout::CANVAS . ' find withtag ' . Layout::PLACEHOLDER_TAG)) === '',
    'placeholder item is gone after the first presentRows',
);
check(
    $tk->evaluate('expr {[lsearch -exact [image names] ' . Layout::PLACEHOLDER_IMAGE . '] < 0}') === '1',
    'placeholder photo is deleted after the first presentRows',
);

$statusText = 'status {braces} "quotes" $dollar [brackets] \\slash';
$tk->status($statusText);
check(
    $tk->evaluate(Layout::STATUS . ' cget -text') === $statusText,
    'status text survives Tcl quoting',
);

$previewWidth = intdiv(IMAGE_WIDTH, 4);
$previewHeight = intdiv(IMAGE_HEIGHT, 4);
$tk->setImageMode(true);
$previewBytes = gradientBytes($previewWidth, $previewHeight, true);
for ($rowBegin = 0; $rowBegin < $previewHeight; $rowBegin += 50) {
    $rowEnd = min($rowBegin + 50, $previewHeight);
    $tk->presentRows(
        substr($previewBytes, $rowBegin * $previewWidth * 4, ($rowEnd - $rowBegin) * $previewWidth * 4),
        $rowBegin,
        $rowEnd,
        $previewWidth,
        true,
    );
}
$tk->status('smoke test: preview bands submitted');
pumpFor($tk, 0.25);

$fullItem = $tk->evaluate('.canvas find withtag full');
$previewItem = $tk->evaluate('.canvas find withtag preview');
check(
    $tk->evaluate('.canvas find overlapping 0 0 1 1') === $fullItem . ' ' . $previewItem,
    'preview layer is on top while previewing',
);
$tk->setImageMode(true);
check(
    $tk->evaluate('.canvas find overlapping 0 0 1 1') === $fullItem . ' ' . $previewItem,
    'setImageMode(true) shows the preview',
);

$previewPath = $outDir . '/tk-smoke-preview.png';
$tk->evaluate(Layout::PREVIEW_IMAGE . ' write {' . $previewPath . '} -format png');
check(pngSize($previewPath) === [IMAGE_WIDTH, IMAGE_HEIGHT, IMAGETYPE_PNG], 'preview photo written as 4x zoomed PNG');

$fullBytes = gradientBytes(IMAGE_WIDTH, IMAGE_HEIGHT, false);
for ($rowBegin = 0; $rowBegin < IMAGE_HEIGHT; $rowBegin += 64) {
    $rowEnd = min($rowBegin + 64, IMAGE_HEIGHT);
    $tk->presentRows(
        substr($fullBytes, $rowBegin * IMAGE_WIDTH * 4, ($rowEnd - $rowBegin) * IMAGE_WIDTH * 4),
        $rowBegin,
        $rowEnd,
        IMAGE_WIDTH,
        false,
    );
}
$tk->status('smoke test: full bands submitted');
pumpFor($tk, 1.0);

check(
    $tk->evaluate('.canvas find overlapping 0 0 1 1') === $fullItem . ' ' . $previewItem,
    'preview layer stays on top while full bands are hidden',
);
$tk->setImageMode(false);
check(
    $tk->evaluate('.canvas find overlapping 0 0 1 1') === $previewItem . ' ' . $fullItem,
    'setImageMode(false) reveals the full image',
);
$tk->setImageMode(true);
check(
    $tk->evaluate('.canvas find overlapping 0 0 1 1') === $fullItem . ' ' . $previewItem,
    'setImageMode(true) shows the preview again',
);
$tk->setImageMode(false);
check(
    $tk->evaluate('.canvas find overlapping 0 0 1 1') === $previewItem . ' ' . $fullItem,
    'setImageMode(false) shows the full image',
);

$fullPath = $outDir . '/tk-smoke-full.png';
$tk->evaluate(Layout::FULL_IMAGE . ' write {' . $fullPath . '} -format png');
check(pngSize($fullPath) === [IMAGE_WIDTH, IMAGE_HEIGHT, IMAGETYPE_PNG], 'full photo written as PNG');

check($tk->evaluate(Layout::FULL_IMAGE . ' get 0 0') === '0 0 0', 'full photo top left pixel is black');
check(
    $tk->evaluate(Layout::FULL_IMAGE . ' get ' . (IMAGE_WIDTH - 1) . ' ' . (IMAGE_HEIGHT - 1)) === '255 255 255',
    'full photo bottom right pixel is white',
);
check($tk->evaluate(Layout::PREVIEW_IMAGE . ' get 0 0') === '255 0 0', 'preview photo top left pixel is red');
check(
    $tk->evaluate(Layout::PREVIEW_IMAGE . ' get 400 0') === '176 0 79',
    'preview photo is zoomed 4x horizontally',
);
check(
    $tk->evaluate(Layout::FULL_IMAGE . ' get 640 338') === '127 127 127',
    'full photo middle pixel matches the BGRA gradient',
);
check(
    filesize($fullPath) > 1024 && md5_file($fullPath) !== md5_file($previewPath),
    'full and preview PNG files differ',
);

$tk->close();
echo "PASS: Tk smoke test\n";
echo 'full PNG: ', $fullPath, "\n";
echo 'preview PNG: ', $previewPath, "\n";
