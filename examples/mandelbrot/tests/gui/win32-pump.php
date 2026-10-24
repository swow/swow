#!/usr/bin/env php
<?php

declare(strict_types=1);

/*
* Standalone Win32 pump responsiveness diagnostic.
*
* This test uses only the Win32 backend: no swow extension, no App, no
* workers. It opens the window, submits one synthetic preview gradient and
* one full gradient, then runs the pre-wait loop shape the App used:
*
*   pump(); usleep(1000);
*
* for about ten seconds and prints one line per second. It is the baseline
* for comparing against an App build that calls Backend::wait() instead.
*
* Run on 64-bit Windows with FFI and zlib enabled:
*
*   php -d ffi.enable=1 tests\gui\win32-pump.php
*
* If FFI is not loaded, enable extension=ffi in php.ini.
*
* While the ten seconds run:
*   1. drag the window around the desktop by its title bar
*   2. click the image area and a non-Quit button
*   3. watch whether the window keeps moving and repainting
*
* Avoid the Quit button; it ends the test early. The other buttons only queue
* events here, because no App is running. Dragging can raise max_pump because
* Windows runs its move loop inside message dispatch; the useful signal is
* that ticks continue after the drag and the window follows the mouse.
*
* Each line reports per-second ticks, the longest pump() call, the longest
* usleep(1000) call, and messages dispatched by pump() in that second.
*
* Send the printed lines and state whether the window stayed responsive. If
* the process itself freezes, report at which second the output stopped.
*
* On a non-Windows host this prints SKIP and exits 0, like win32-smoke.php.
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

const WINDOW_WIDTH = 1280;
const WINDOW_HEIGHT = 720;
const IMAGE_WIDTH = WINDOW_WIDTH;
const IMAGE_HEIGHT = WINDOW_HEIGHT - Layout::TOOLBAR_HEIGHT;
const PREVIEW_ZOOM = 4;
const DURATION_SECONDS = 10.0;
const TICK_SLEEP_US = 1000;

function fail(string $message): never
{
    fwrite(STDERR, 'FAIL: ' . $message . "\n");
    exit(1);
}

function pass(string $message): void
{
    echo 'PASS: ', $message, "\n";
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

function pushBands(
    Win32 $win,
    string $bytes,
    int $width,
    int $height,
    int $bandRows,
    bool $preview,
): void {
    for ($row = 0; $row < $height; $row += $bandRows) {
        $end = min($row + $bandRows, $height);
        $win->presentRows(
            substr($bytes, $row * $width * 4, ($end - $row) * $width * 4),
            $row,
            $end,
            $width,
            $preview,
        );
    }
}

/**
 * Pump without timing, used to paint the setup frames before the measurement.
 */
function pumpFor(Win32 $win, float $seconds): void
{
    $deadline = microtime(true) + $seconds;
    while (microtime(true) < $deadline) {
        $win->pump();
        usleep(TICK_SLEEP_US);
    }
}

if (PHP_OS_FAMILY !== 'Windows') {
    echo "SKIP: window checks need 64-bit Windows\n";
    pass('Win32 pump diagnostic skipped on this host');
    exit(0);
}

if (!extension_loaded('ffi')) {
    fail('FFI is not loaded; run with -d ffi.enable=1 and enable extension=ffi');
}
if (PHP_INT_SIZE < 8) {
    fail('the Win32 backend needs 64-bit PHP');
}

$win = new Win32();
$open = false;
$exitCode = 0;

try {
    $win->open(WINDOW_WIDTH, WINDOW_HEIGHT);
    $open = true;

    $previewWidth = intdiv(IMAGE_WIDTH, PREVIEW_ZOOM);
    $previewHeight = intdiv(IMAGE_HEIGHT, PREVIEW_ZOOM);

    $win->setImageMode(true);
    pushBands(
        $win,
        gradientBytes($previewWidth, $previewHeight, true),
        $previewWidth,
        $previewHeight,
        25,
        true,
    );
    pumpFor($win, 0.15);

    /*
    * Submit full bands while the preview is still the visible layer, as the
    * App does; setImageMode(false) then paints the completed image once
    */
    pushBands(
        $win,
        gradientBytes(IMAGE_WIDTH, IMAGE_HEIGHT, false),
        IMAGE_WIDTH,
        IMAGE_HEIGHT,
        64,
        false,
    );
    $win->setImageMode(false);

    $win->setFields('-0.743643887037151', '0.13182590420533', '1e4');
    $win->status('pump diagnostic: drag and click the window for 10 seconds');
    pumpFor($win, 0.2);
    pass('window opened; preview and full gradients submitted');

    $initialMessages = $win->dispatchedMessages();
    $lastMessages = $initialMessages;
    $start = microtime(true);
    $deadline = $start + DURATION_SECONDS;
    $nextReport = $start + 1.0;
    $ticks = 0;
    $totalTicks = 0;
    $maxPumpMs = 0.0;
    $maxSleepMs = 0.0;
    $overallMaxPumpMs = 0.0;
    $overallMaxSleepMs = 0.0;
    $quit = false;

    while (microtime(true) < $deadline) {
        $operationStart = microtime(true);
        $events = $win->pump();
        $pumpMs = (microtime(true) - $operationStart) * 1000.0;
        $maxPumpMs = max($maxPumpMs, $pumpMs);
        $overallMaxPumpMs = max($overallMaxPumpMs, $pumpMs);

        foreach ($events as $event) {
            if (($event['type'] ?? '') === 'quit') {
                $quit = true;
            }
        }
        if ($quit) {
            pass('quit event received; stopping early');
            break;
        }

        $operationStart = microtime(true);
        usleep(TICK_SLEEP_US);
        $sleepMs = (microtime(true) - $operationStart) * 1000.0;
        $maxSleepMs = max($maxSleepMs, $sleepMs);
        $overallMaxSleepMs = max($overallMaxSleepMs, $sleepMs);

        $ticks++;
        $totalTicks++;
        $now = microtime(true);
        if ($now < $nextReport) {
            continue;
        }

        $second = max(1, (int) floor($now - $start));
        $messages = $win->dispatchedMessages();
        pass(sprintf(
            'second %d ticks=%d max_pump=%.2f ms max_sleep=%.2f ms messages=%d',
            $second,
            $ticks,
            $maxPumpMs,
            $maxSleepMs,
            $messages - $lastMessages,
        ));
        $ticks = 0;
        $maxPumpMs = 0.0;
        $maxSleepMs = 0.0;
        $lastMessages = $messages;
        $nextReport = $now + 1.0;
    }

    $messages = $win->dispatchedMessages();
    pass(sprintf(
        'total ticks=%d max_pump=%.2f ms max_sleep=%.2f ms messages=%d',
        $totalTicks,
        $overallMaxPumpMs,
        $overallMaxSleepMs,
        $messages - $initialMessages,
    ));

    $win->close();
    $open = false;
    pass('Win32 pump diagnostic finished');
} catch (Throwable $error) {
    $exitCode = 1;
    fwrite(STDERR, 'FAIL: ' . $error->getMessage() . "\n");
} finally {
    if ($open) {
        try {
            $win->close();
        } catch (Throwable) {
        }
    }
}

exit($exitCode);
