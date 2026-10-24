#!/usr/bin/env php
<?php

declare(strict_types=1);

/*
* End-to-end interaction test for a running app instance.
*
* Start the app first, with DISPLAY set, for example:
*
*   Xvfb :99 -screen 0 1400x900x24 -nolisten tcp &
*   DISPLAY=:99 php -d extension=swow \
*     bin/mandelbrot --threads=4 --depth=1e4 --output=interaction.png &
*   DISPLAY=:99 php tests/gui/drive.php
*
* The app must start at the default view and depth 1e4. The test opens a
* hidden Tk client, finds the app through winfo interps, and drives the real
* zoomIn, reset, apply and quit commands over Tk send. It exits 0 only when
* every step passes and the app has quit; otherwise it exits non-zero.
*/

$root = dirname(__DIR__, 2);
require_once $root . '/src/Gui/Backend.php';
require_once $root . '/src/Gui/Layout.php';
require_once $root . '/src/Gui/Tk.php';

use Swow\Mandelbrot\Gui\Layout;
use Swow\Mandelbrot\Gui\Tk;

function fail(string $message): never
{
    throw new RuntimeException($message);
}

function note(string $message): void
{
    echo $message, "\n";
}

function tclName(string $name): string
{
    return '{' . str_replace(['\\', '{', '}'], ['\\\\', '\{', '\}'], $name) . '}';
}

function remote(Tk $tk, string $target, string $script): string
{
    return $tk->evaluate('send ' . tclName($target) . ' {' . $script . '}');
}

function status(Tk $tk, string $target): string
{
    return remote($tk, $target, '.toolbar.status cget -text');
}

function field(Tk $tk, string $target, string $name): string
{
    return remote($tk, $target, Layout::TOOLBAR . '.' . $name . ' get');
}

function background(Tk $tk, string $target, string $name): string
{
    return remote($tk, $target, Layout::TOOLBAR . '.' . $name . ' cget -background');
}

/** @return array{re: string, im: string, depth: string} */
function fields(Tk $tk, string $target): array
{
    return [
        're' => field($tk, $target, 're'),
        'im' => field($tk, $target, 'im'),
        'depth' => field($tk, $target, 'depth'),
    ];
}

/** @return array{re: string, im: string, depth: string} */
function backgrounds(Tk $tk, string $target): array
{
    return [
        're' => background($tk, $target, 're'),
        'im' => background($tk, $target, 'im'),
        'depth' => background($tk, $target, 'depth'),
    ];
}

function expect(string $actual, string $expected, string $what): void
{
    if ($actual !== $expected) {
        fail($what . ': expected [' . $expected . '], got [' . $actual . ']');
    }
    note('    ' . $what . ' = ' . $actual);
}

function waitReady(Tk $tk, string $target, float $timeout = 30.0): void
{
    $start = microtime(true);
    $last = null;
    $readySince = null;
    $busy = false;
    while (microtime(true) - $start < $timeout) {
        $value = status($tk, $target);
        if ($value !== $last) {
            printf("    status: %s (%.3f s)\n", $value, microtime(true) - $start);
            $last = $value;
        }
        if (!str_starts_with($value, 'ready')) {
            $busy = true;
            $readySince = null;
        } else {
            $readySince ??= microtime(true);
            if (($busy && microtime(true) - $readySince >= 0.4) ||
                (!$busy && microtime(true) - $readySince >= 1.0)) {
                return;
            }
        }
        usleep(50_000);
    }
    fail('timeout waiting for ready; last status ' . ($last ?? '<none>'));
}

function waitField(Tk $tk, string $target, string $name, string $expected, float $timeout = 10.0): void
{
    $deadline = microtime(true) + $timeout;
    do {
        $value = field($tk, $target, $name);
        if ($value === $expected) {
            return;
        }
        usleep(50_000);
    } while (microtime(true) < $deadline);
    fail($name . ' never became [' . $expected . '], last [' . $value . ']');
}

function waitBackground(Tk $tk, string $target, string $name, string $expected, float $timeout = 5.0): void
{
    $deadline = microtime(true) + $timeout;
    do {
        $value = background($tk, $target, $name);
        if ($value === $expected) {
            return;
        }
        usleep(50_000);
    } while (microtime(true) < $deadline);
    fail($name . ' background never became [' . $expected . '], last [' . $value . ']');
}

function appPresent(Tk $tk, string $target): bool
{
    $count = (int) $tk->evaluate('llength [winfo interps]');
    for ($i = 0; $i < $count; $i++) {
        if ($tk->evaluate('lindex [winfo interps] ' . $i) === $target) {
            return true;
        }
    }

    return false;
}

function samplePixels(Tk $tk, string $target): string
{
    $script = 'set s {}; ';
    foreach ([[10, 10], [320, 170], [640, 338], [960, 500], [1270, 670]] as [$x, $y]) {
        $script .= 'lappend s [mandel_full get ' . $x . ' ' . $y . ']; ';
    }
    $script .= 'set s';

    return remote($tk, $target, $script);
}

$tk = new Tk();
$target = null;
$quitSent = false;
$exit = 1;

try {
    $tk->open(320, 240);
    $tk->evaluate('wm withdraw .');
    $own = $tk->evaluate('tk appname');

    $discoverDeadline = microtime(true) + 10.0;
    do {
        $count = (int) $tk->evaluate('llength [winfo interps]');
        for ($i = 0; $i < $count; $i++) {
            $name = $tk->evaluate('lindex [winfo interps] ' . $i);
            if ($name === $own) {
                continue;
            }
            try {
                $depth = remote($tk, $name, '.toolbar.depth get');
            } catch (RuntimeException) {
                continue;
            }
            if ($depth !== '') {
                $target = $name;
                break 2;
            }
        }
        usleep(100_000);
    } while (microtime(true) < $discoverDeadline);

    if ($target === null) {
        fail('no target Tk application found');
    }
    note('target=' . $target . ' own=' . $own);

    note('step 1: wait for initial ready');
    waitReady($tk, $target);
    $initialFields = fields($tk, $target);
    $initialBackgrounds = backgrounds($tk, $target);
    note('    initial fields: re=' . $initialFields['re'] . ' im=' . $initialFields['im']
        . ' depth=' . $initialFields['depth']);
    note('    initial backgrounds: re=' . $initialBackgrounds['re'] . ' im=' . $initialBackgrounds['im']
        . ' depth=' . $initialBackgrounds['depth']);
    expect($initialFields['depth'], '10000', 'initial depth');
    $initialPixels = samplePixels($tk, $target);
    note('    initial pixel sample: ' . $initialPixels);

    note('step 2: ::mandel::zoomIn');
    remote($tk, $target, '::mandel::zoomIn');
    waitField($tk, $target, 'depth', '20000');
    waitReady($tk, $target);
    $zoomFields = fields($tk, $target);
    expect($zoomFields['depth'], '20000', 'depth after zoomIn');
    expect($zoomFields['re'], $initialFields['re'], 'real centre after zoomIn');
    expect($zoomFields['im'], $initialFields['im'], 'imag centre after zoomIn');

    note('step 3: ::mandel::reset');
    remote($tk, $target, '::mandel::reset');
    waitField($tk, $target, 'depth', '10000');
    waitReady($tk, $target);
    $resetFields = fields($tk, $target);
    expect($resetFields['re'], $initialFields['re'], 'real centre after reset');
    expect($resetFields['im'], $initialFields['im'], 'imag centre after reset');
    expect($resetFields['depth'], '10000', 'depth after reset');
    $resetPixels = samplePixels($tk, $target);
    expect($resetPixels, $initialPixels, 'image returned to the initial frame after reset');

    note('step 4: invalid depth abc + apply');
    remote($tk, $target, '.toolbar.depth delete 0 end; .toolbar.depth insert 0 abc');
    remote($tk, $target, '::mandel::apply');
    waitBackground($tk, $target, 'depth', '#ff6666');
    $invalidBackgrounds = backgrounds($tk, $target);
    $invalidFields = fields($tk, $target);
    $invalidStatus = status($tk, $target);
    note('    invalid backgrounds: re=' . $invalidBackgrounds['re'] . ' im=' . $invalidBackgrounds['im']
        . ' depth=' . $invalidBackgrounds['depth']);
    note('    invalid status: ' . $invalidStatus);
    note('    invalid fields: re=' . $invalidFields['re'] . ' im=' . $invalidFields['im']
        . ' depth=' . $invalidFields['depth']);
    expect($invalidBackgrounds['depth'], '#ff6666', 'depth entry background after invalid apply');
    expect($invalidBackgrounds['re'], $initialBackgrounds['re'], 'real background unchanged after invalid apply');
    expect($invalidBackgrounds['im'], $initialBackgrounds['im'], 'imag background unchanged after invalid apply');
    expect($invalidFields['re'], $resetFields['re'], 'real field after invalid apply');
    expect($invalidFields['im'], $resetFields['im'], 'imag field after invalid apply');
    expect($invalidFields['depth'], 'abc', 'depth field retains the invalid text');
    if ($invalidStatus === '' || stripos($invalidStatus, 'depth') === false) {
        fail('status does not explain the depth error: [' . $invalidStatus . ']');
    }
    $invalidPixels = samplePixels($tk, $target);
    expect($invalidPixels, $resetPixels, 'image unchanged after invalid apply');

    note('step 5: valid centre + depth 1e5, apply');
    remote($tk, $target, '.toolbar.re delete 0 end; .toolbar.re insert 0 {-0.743643887018689113498922249712800000007037228233600378464531}');
    remote($tk, $target, '.toolbar.im delete 0 end; .toolbar.im insert 0 {0.131825904211894657975878336475200000004669779776843260798656}');
    remote($tk, $target, '.toolbar.depth delete 0 end; .toolbar.depth insert 0 {1e5}');
    remote($tk, $target, '::mandel::apply');
    waitField($tk, $target, 'depth', '100000');
    waitReady($tk, $target);
    $validFields = fields($tk, $target);
    $validBackgrounds = backgrounds($tk, $target);
    expect($validFields['depth'], '100000', 'depth after valid apply');
    expect($validFields['re'], '-0.743643887018689113498922249712800000007037228233600378464531', 'real field after valid apply');
    expect($validFields['im'], '0.131825904211894657975878336475200000004669779776843260798656', 'imag field after valid apply');
    expect($validBackgrounds['depth'], $initialBackgrounds['depth'], 'depth invalid mark cleared');
    expect($validBackgrounds['re'], $initialBackgrounds['re'], 'real invalid mark cleared');
    expect($validBackgrounds['im'], $initialBackgrounds['im'], 'imag invalid mark cleared');
    $readyStatus = status($tk, $target);
    if (!str_starts_with($readyStatus, 'ready')) {
        fail('status after valid apply: expected ready, got [' . $readyStatus . ']');
    }
    note('    status after valid apply: ' . $readyStatus);
    $validPixels = samplePixels($tk, $target);
    if ($validPixels === $resetPixels) {
        fail('image pixels did not change after the valid apply');
    }
    note('    pixel sample after apply: ' . $validPixels);

    note('step 6: ::mandel::quit');
    try {
        remote($tk, $target, '::mandel::quit');
        note('    quit command returned');
    } catch (RuntimeException $error) {
        note('    quit send ended: ' . $error->getMessage());
    }
    $quitSent = true;

    $goneDeadline = microtime(true) + 10.0;
    while (microtime(true) < $goneDeadline && appPresent($tk, $target)) {
        usleep(50_000);
    }
    if (appPresent($tk, $target)) {
        fail('app still registered after quit');
    }
    note('    app interp is gone');

    note('PASS: live interaction test');
    $exit = 0;
} catch (Throwable $error) {
    fwrite(STDERR, 'FAIL: ' . $error->getMessage() . "\n");
    if ($target !== null && !$quitSent) {
        try {
            remote($tk, $target, '::mandel::quit');
        } catch (Throwable) {
        }
    }
} finally {
    try {
        $tk->close();
    } catch (Throwable) {
    }
}

exit($exit);
