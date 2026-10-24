#!/usr/bin/env php
<?php

declare(strict_types=1);

/*
* Win32 backend smoke test.
*
* Run on 64-bit Windows 10 version 1703 or newer, with PHP 8.7 or newer and
* the FFI and zlib extensions enabled:
*
*   php -d ffi.enable=1 tests\gui\win32-smoke.php
*
* If FFI is not loaded, enable extension=ffi in php.ini. A typical Windows
* build ships php_ffi.dll next to php.exe.
*
* This test needs no swow extension: the GUI layer uses FFI only. The demo app
* itself does need swow, and its bootstrap requires a ZTS build, so use the
* thread safe (TS) Windows package, for example:
*
*   php -d extension=swow -d ffi.enable=1 bin\mandelbrot --threads=4 --depth=1e4
*
* If php.ini already loads swow, drop the -d option. Otherwise install
* php_swow.dll from the swow release page and add extension=swow to php.ini,
* or pass the file name directly with -d extension=php_swow.dll. The DLL must
* match the PHP version, architecture and thread safety of php.exe.
*
* On a non-Windows host only the pure PNG decoder and argument packing checks
* run; the window checks are skipped. Do not click the window while the test
* runs: stray input events fail the pump checks.
*/

$root = dirname(__DIR__, 2);
require_once $root . '/src/Gui/Backend.php';
require_once $root . '/src/Gui/Layout.php';
require_once $root . '/src/Gui/Win32/Png.php';
require_once $root . '/src/Gui/Win32/Text.php';
require_once $root . '/src/Gui/Win32/Dialog.php';
require_once $root . '/src/Gui/Win32.php';

use Swow\Mandelbrot\Gui\Layout;
use Swow\Mandelbrot\Gui\Win32;
use Swow\Mandelbrot\Gui\Win32\Png;
use Swow\Mandelbrot\Gui\Win32\Text;

const WINDOW_WIDTH = 1280;
const WINDOW_HEIGHT = 720;
const IMAGE_WIDTH = WINDOW_WIDTH;
const IMAGE_HEIGHT = WINDOW_HEIGHT - Layout::TOOLBAR_HEIGHT;
const PREVIEW_ZOOM = 4;

$failures = 0;

function check(bool $condition, string $message): void
{
    global $failures;

    if ($condition) {
        echo 'PASS: ', $message, "\n";

        return;
    }
    $failures++;
    echo 'FAIL: ', $message, "\n";
}

function finish(string $title): never
{
    global $failures;

    if ($failures > 0) {
        echo 'FAIL: ', $title, ' (', $failures, " failed check(s))\n";
        exit(1);
    }
    echo 'PASS: ', $title, "\n";
    exit(0);
}

function pngChunk(string $type, string $data): string
{
    return pack('N', strlen($data))
        . $type
        . $data
        . pack('N', crc32($type . $data));
}

function paeth(int $left, int $up, int $upLeft): int
{
    $estimate = $left + $up - $upLeft;
    $leftDistance = abs($estimate - $left);
    $upDistance = abs($estimate - $up);
    $upLeftDistance = abs($estimate - $upLeft);

    if ($leftDistance <= $upDistance && $leftDistance <= $upLeftDistance) {
        return $left;
    }

    return $upDistance <= $upLeftDistance ? $up : $upLeft;
}

/**
 * PNG filter encoding, used to build a fixture that covers all five filters.
 */
function filterRow(string $raw, string $previous, int $filter, int $bpp): string
{
    $out = '';
    $length = strlen($raw);

    for ($i = 0; $i < $length; $i++) {
        $value = ord($raw[$i]);
        $left = $i >= $bpp ? ord($raw[$i - $bpp]) : 0;
        $up = ord($previous[$i]);
        $upLeft = $i >= $bpp ? ord($previous[$i - $bpp]) : 0;
        $predictor = match ($filter) {
            0 => 0,
            1 => $left,
            2 => $up,
            3 => ($left + $up) >> 1,
            4 => paeth($left, $up, $upLeft),
        };
        $out .= chr(($value - $predictor) & 0xFF);
    }

    return $out;
}

/**
 * Build an RGB fixture PNG whose rows use filters 0 to 4.
 *
 * @return array{0: string, 1: string} PNG bytes and expected BGRA pixels
 */
function filterFixturePng(): array
{
    $width = 4;
    $height = 5;
    $rows = [];
    for ($y = 0; $y < $height; $y++) {
        $row = '';
        for ($x = 0; $x < $width; $x++) {
            $row .= chr((17 * $x + 5 * $y) & 0xFF)
                . chr((31 * $x + 11 * $y) & 0xFF)
                . chr((47 * $x + 23 * $y) & 0xFF);
        }
        $rows[] = $row;
    }

    $raw = '';
    $previous = str_repeat("\0", $width * 3);
    foreach ($rows as $filter => $row) {
        $raw .= chr($filter) . filterRow($row, $previous, $filter, 3);
        $previous = $row;
    }

    $png = "\x89PNG\r\n\x1a\n"
        . pngChunk('IHDR', pack('NNCCCCC', $width, $height, 8, 2, 0, 0, 0))
        . pngChunk('IDAT', gzcompress($raw, 6))
        . pngChunk('IEND', '');

    $expected = '';
    for ($y = 0; $y < $height; $y++) {
        $row = $rows[$y];
        for ($x = 0; $x < $width; $x++) {
            $offset = $x * 3;
            $expected .= $row[$offset + 2] . $row[$offset + 1] . $row[$offset] . "\xFF";
        }
    }

    return [$png, $expected];
}

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

function pumpFor(Win32 $win, float $seconds): void
{
    global $failures;

    $deadline = microtime(true) + $seconds;
    while (microtime(true) < $deadline) {
        $events = $win->pump();
        foreach ($events as $event) {
            $failures++;
            echo 'FAIL: unexpected event without input: ', json_encode($event), "\n";
        }
        usleep(10_000);
    }
}

/*
* Pure helpers. These run everywhere and cover the PNG decoder and the
* WM_COMMAND argument packing used by the WndProc.
*/

check(Text::toUtf16Le('A') === "A\0", 'UTF-16LE encodes ASCII');
check(Text::toUtf16Le('中') === "\x2D\x4E", 'UTF-16LE encodes a BMP code point');
check(
    Text::toUtf16Le("\u{1F600}") === "\x3D\xD8\x00\xDE",
    'UTF-16LE encodes a surrogate pair',
);
check(
    Text::fromUtf16Le("A\0\x2D\x4E\x3D\xD8\x00\xDE") === "A中\u{1F600}",
    'UTF-16LE round trip handles ASCII, BMP and astral text',
);
check(
    Text::fromUtf16Le("A\0\0\0B\0") === 'A',
    'UTF-16LE decoding stops at the first NUL',
);

check(Win32::commandWParam(107, 0) === 107, 'WM_COMMAND packs the button id');
check(
    Win32::commandWParam(0x1234, 0x0300) === 0x03001234,
    'WM_COMMAND packs the notification code in the high word',
);
check(Win32::commandId(0x03001234) === 0x1234, 'WM_COMMAND decodes the id');
check(Win32::commandCode(0x03001234) === 0x0300, 'WM_COMMAND decodes the code');

[$fixturePng, $expectedPixels] = filterFixturePng();
$fixture = Png::decode($fixturePng);
check($fixture['width'] === 4 && $fixture['height'] === 5, 'PNG fixture keeps its dimensions');
check($fixture['bgra'] === $expectedPixels, 'PNG decoder handles all five filters');

$placeholderPath = Layout::placeholderPath();
$placeholderPng = file_get_contents($placeholderPath);
check($placeholderPng !== false, 'placeholder image is readable');
if ($placeholderPng !== false) {
    $placeholder = Png::decode($placeholderPng);
    check(
        $placeholder['width'] === IMAGE_WIDTH && $placeholder['height'] === IMAGE_HEIGHT,
        sprintf(
            'placeholder decodes to %dx%d',
            IMAGE_WIDTH,
            IMAGE_HEIGHT,
        ),
    );
    $pixels = $placeholder['bgra'];
    check(
        strlen($pixels) === IMAGE_WIDTH * IMAGE_HEIGHT * 4,
        'placeholder decodes to a tightly packed BGRA image',
    );
    $opaque = true;
    for ($i = 3, $size = strlen($pixels); $i < $size; $i += 4) {
        if ($pixels[$i] !== "\xFF") {
            $opaque = false;
            break;
        }
    }
    check($opaque, 'placeholder alpha is 255 everywhere');
}

$rejected = false;
try {
    Png::decode('not a PNG');
} catch (InvalidArgumentException) {
    $rejected = true;
}
check($rejected, 'PNG decoder rejects a file without the signature');

if (PHP_OS_FAMILY !== 'Windows') {
    echo "SKIP: window checks need 64-bit Windows\n";
    finish('Win32 decoder and packing tests');
}

if (!extension_loaded('ffi')) {
    echo "FAIL: FFI is not loaded; run with -d ffi.enable=1 and enable extension=ffi\n";
    exit(1);
}
if (PHP_INT_SIZE < 8) {
    echo "FAIL: the Win32 backend needs 64-bit PHP\n";
    exit(1);
}

$win = new Win32();
check(
    $win instanceof Swow\Mandelbrot\Gui\Backend,
    'Win32 implements the Backend interface',
);
$open = false;
try {
    $win->open(WINDOW_WIDTH, WINDOW_HEIGHT);
    $open = true;
    pumpFor($win, 0.1);

    check($win->clientSize() === [WINDOW_WIDTH, WINDOW_HEIGHT], 'client area is 1280x720');
    check($win->imageWidth() === IMAGE_WIDTH, 'imageWidth() is 1280');
    check($win->imageHeight() === IMAGE_HEIGHT, 'imageHeight() is 676');
    check($win->windowTitle() === Layout::TITLE, 'window title is ' . Layout::TITLE);

    $autoTitle = Layout::TITLE . ' (auto mode)';
    $win->setTitle($autoTitle);
    check($win->windowTitle() === $autoTitle, 'setTitle updates the window title');
    $win->setTitle(Layout::TITLE);
    check($win->windowTitle() === Layout::TITLE, 'setTitle restores the window title');

    check($win->placeholderVisible(), 'placeholder is visible before the first frame');
    check(!$win->imageMode(), 'full image mode is selected at open');

    $win->setImageMode(true);
    pumpFor($win, 0.05);
    check($win->imageMode(), 'setImageMode(true) selects the preview');
    check($win->placeholderVisible(), 'placeholder survives the preview mode switch');

    $previewWidth = intdiv(IMAGE_WIDTH, PREVIEW_ZOOM);
    $previewHeight = intdiv(IMAGE_HEIGHT, PREVIEW_ZOOM);
    $preview = gradientBytes($previewWidth, $previewHeight, true);
    for ($rowBegin = 0; $rowBegin < $previewHeight; $rowBegin += 25) {
        $rowEnd = min($rowBegin + 25, $previewHeight);
        $win->presentRows(
            substr($preview, $rowBegin * $previewWidth * 4, ($rowEnd - $rowBegin) * $previewWidth * 4),
            $rowBegin,
            $rowEnd,
            $previewWidth,
            true,
        );
    }
    pumpFor($win, 0.3);
    check(!$win->placeholderVisible(), 'placeholder is removed after the first preview bands');
    check($win->imageMode(), 'preview stays selected while preview bands arrive');

    $win->setImageMode(false);
    pumpFor($win, 0.05);
    check(!$win->imageMode(), 'setImageMode(false) selects the full image');

    $full = gradientBytes(IMAGE_WIDTH, IMAGE_HEIGHT, false);
    for ($rowBegin = 0; $rowBegin < IMAGE_HEIGHT; $rowBegin += 64) {
        $rowEnd = min($rowBegin + 64, IMAGE_HEIGHT);
        $win->presentRows(
            substr($full, $rowBegin * IMAGE_WIDTH * 4, ($rowEnd - $rowBegin) * IMAGE_WIDTH * 4),
            $rowBegin,
            $rowEnd,
            IMAGE_WIDTH,
            false,
        );
    }
    pumpFor($win, 1.0);
    check(!$win->placeholderVisible(), 'placeholder stays removed after full bands');
    check(!$win->imageMode(), 'full image is shown after the full bands');

    $win->setFields('-0.743643887037151', '0.13182590420533', '1e4');
    check($win->fieldText('re') === '-0.743643887037151', 'setFields fills the real entry');
    check($win->fieldText('im') === '0.13182590420533', 'setFields fills the imaginary entry');
    check($win->fieldText('depth') === '1e4', 'setFields fills the depth entry');

    $win->markInvalid('depth', 'invalid depth');
    check($win->invalidField() === 'depth', 'markInvalid marks the depth entry');
    check($win->statusText() === 'invalid depth', 'markInvalid writes the status line');
    check($win->fieldText('depth') === '1e4', 'markInvalid keeps the entry text');
    $win->markInvalid(null, '');
    check($win->invalidField() === null, 'markInvalid(null) clears the mark');
    check($win->statusText() === '', 'markInvalid(null) clears the status line');

    $rejected = false;
    try {
        $win->markInvalid('center', 'x');
    } catch (InvalidArgumentException) {
        $rejected = true;
    }
    check($rejected, 'markInvalid rejects an unknown field');

    $statusText = 'status {braces} "quotes" $dollar \slash 中文';
    $win->status($statusText);
    check($win->statusText() === $statusText, 'status text survives UTF-16 conversion');

    $rejected = false;
    try {
        $win->presentRows('short', 0, 1, IMAGE_WIDTH, false);
    } catch (InvalidArgumentException) {
        $rejected = true;
    }
    check($rejected, 'presentRows rejects a short payload');

    $busyWidgets = [
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
    ];
    $win->setBusy(true);
    foreach ($busyWidgets as $component) {
        check(!$win->widgetEnabled($component), $component . ' is disabled while busy');
    }
    check($win->widgetEnabled(Layout::COMP_QUIT), 'Quit stays enabled while busy');
    $win->setBusy(true);
    check(!$win->widgetEnabled(Layout::COMP_GO), 'a second setBusy(true) is a no-op');
    $win->setBusy(false);
    foreach ($busyWidgets as $component) {
        check($win->widgetEnabled($component), $component . ' is enabled after setBusy(false)');
    }
    check($win->widgetEnabled(Layout::COMP_QUIT), 'Quit stays enabled after setBusy(false)');

    $waitOk = true;
    try {
        $win->wait(0);
        $win->wait(1000);
    } catch (Throwable) {
        $waitOk = false;
    }
    check($waitOk, 'wait() completes without throwing');

    $win->close();
    $open = false;
    check(true, 'close() released the window');

    $closed = false;
    try {
        $win->imageWidth();
    } catch (LogicException) {
        $closed = true;
    }
    check($closed, 'calls after close() throw LogicException');

    $setTitleOk = true;
    try {
        $win->setTitle(Layout::TITLE . ' (auto mode)');
    } catch (Throwable) {
        $setTitleOk = false;
    }
    check($setTitleOk, 'setTitle after close() is a no-op and does not throw');
} catch (Throwable $error) {
    $failures++;
    echo 'FAIL: exception: ', $error->getMessage(), "\n";
} finally {
    if ($open) {
        try {
            $win->close();
        } catch (Throwable) {
        }
    }
}

finish('Win32 smoke test');
