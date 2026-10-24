#!/usr/bin/env php
<?php

declare(strict_types=1);

/*
* Event and action test for the Cocoa backend.
*
* Needs macOS with a logged-in console session and a PHP build with FFI:
*   /opt/homebrew/bin/php tests/gui/cocoa-events.php
*
* The test does not load the swow extension. Save is checked by wiring only:
* asking for a path opens a modal NSSavePanel, which needs a human.
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

$entries = [
    're' => Layout::COMP_REAL,
    'im' => Layout::COMP_IMAG,
    'depth' => Layout::COMP_DEPTH,
];

$cocoa = new Cocoa();
$cocoa->open(WINDOW_WIDTH, WINDOW_HEIGHT);
$cocoa->pump();
check($cocoa->pump() === [], 'event queue starts empty');

$cocoa->setFields('-0.75', '0.1', '1e5');
check($cocoa->fieldValue(Layout::COMP_REAL) === '-0.75', 'setFields syncs the real field');
check($cocoa->fieldValue(Layout::COMP_IMAG) === '0.1', 'setFields syncs the imaginary field');
check($cocoa->fieldValue(Layout::COMP_DEPTH) === '1e5', 'setFields syncs the depth field');

$buttons = [
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
];
foreach ($buttons as $component => $action) {
    check(
        $cocoa->widgetAction($component) === $action,
        $component . ' is bound to ' . $action,
    );
    check($cocoa->widgetIsWired($component), $component . ' targets the PHP controller');
}
foreach ($entries as $field => $component) {
    check($cocoa->widgetAction($component) === 'go:', 'Return on the ' . $field . ' field is bound to apply');
    check($cocoa->widgetIsWired($component), $component . ' targets the PHP controller');
}

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
$cocoa->status('busy probe');
$cocoa->setBusy(true);
foreach ($busyWidgets as $component) {
    check(!$cocoa->widgetEnabled($component), $component . ' is disabled while busy');
}
check($cocoa->widgetEnabled(Layout::COMP_QUIT), 'quit stays enabled while busy');
check($cocoa->statusValue() === 'busy probe', 'status text stays readable while busy');
$cocoa->performClick(Layout::COMP_GO);
check($cocoa->pump() === [], 'a disabled Go button does not emit an event');
$cocoa->setBusy(true);
foreach ($busyWidgets as $component) {
    check(!$cocoa->widgetEnabled($component), $component . ' is still disabled after a second setBusy(true)');
}
$cocoa->setBusy(false);
foreach ($busyWidgets as $component) {
    check($cocoa->widgetEnabled($component), $component . ' is enabled after setBusy(false)');
}
check($cocoa->widgetEnabled(Layout::COMP_QUIT), 'quit stays enabled after setBusy(false)');
check($cocoa->statusValue() === 'busy probe', 'status text survives the busy toggles');

$cocoa->performClick(Layout::COMP_ZOOM_OUT);
$cocoa->performClick(Layout::COMP_ZOOM_IN);
$cocoa->performClick(Layout::COMP_RESET);
$cocoa->performClick(Layout::COMP_PAN_UP);
$cocoa->performClick(Layout::COMP_PAN_DOWN);
$cocoa->performClick(Layout::COMP_PAN_LEFT);
$cocoa->performClick(Layout::COMP_PAN_RIGHT);
$cocoa->performClick(Layout::COMP_GO);
$cocoa->performClick(Layout::COMP_QUIT);

$apply = ['type' => 'apply', 're' => '-0.75', 'im' => '0.1', 'depth' => '1e5'];
$expected = [
    ['type' => 'zoomOut'],
    ['type' => 'zoomIn'],
    ['type' => 'reset'],
    ['type' => 'panUp'],
    ['type' => 'panDown'],
    ['type' => 'panLeft'],
    ['type' => 'panRight'],
    $apply,
    ['type' => 'quit'],
];
$events = $cocoa->pump();
check($events === $expected, 'buttons queue events in order');
check($cocoa->pump() === [], 'pump drains the event queue');

foreach ($entries as $field => $component) {
    check($cocoa->performFieldAction($component), 'Return action on the ' . $field . ' field is delivered');
}
$events = $cocoa->pump();
check($events === [$apply, $apply, $apply], 'Return on each field queues an apply event');

$cocoa->postReturnKey(Layout::COMP_REAL);
check(
    $cocoa->pump() === [$apply],
    'a posted Return key event reaches the field action through pump',
);

$defaultBackgrounds = [];
foreach ($entries as $field => $component) {
    $defaultBackgrounds[$field] = $cocoa->fieldBackground($component);
}
foreach ($entries as $field => $component) {
    $cocoa->markInvalid($field, 'bad ' . $field);
    $background = $cocoa->fieldBackground($component);
    check(
        $background[0] > 0.99 && $background[1] < 0.01 && $background[2] < 0.01,
        'markInvalid paints the ' . $field . ' field red',
    );

    $others = $defaultBackgrounds;
    unset($others[$field]);
    $actual = [];
    foreach ($entries as $name => $otherComponent) {
        if ($name !== $field) {
            $actual[$name] = $cocoa->fieldBackground($otherComponent);
        }
    }
    check($actual === $others, 'markInvalid leaves the other fields alone');
    check($cocoa->statusValue() === 'bad ' . $field, 'markInvalid writes the ' . $field . ' reason to the status line');

    $cocoa->markInvalid(null, '');
    $restored = [];
    foreach ($entries as $name => $otherComponent) {
        $restored[$name] = $cocoa->fieldBackground($otherComponent);
    }
    check($restored === $defaultBackgrounds, 'markInvalid(null) clears the ' . $field . ' mark');
}

try {
    $cocoa->markInvalid('center', 'x');
    fail('unknown invalid field was accepted');
} catch (InvalidArgumentException) {
    echo "ok - unknown invalid field rejected\n";
}

$cocoa->closeWindow();
check($cocoa->pump() === [['type' => 'quit']], 'closing the window queues quit');

$cocoa->close();
echo "PASS: Cocoa event test\n";
