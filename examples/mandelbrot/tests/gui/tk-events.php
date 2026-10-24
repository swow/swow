#!/usr/bin/env php
<?php

declare(strict_types=1);

/*
* Event and command test for the Tk backend.
*
* Needs a running X server, for example:
*   Xvfb :99 -screen 0 1400x900x24 &
*   DISPLAY=:99 php tests/gui/tk-events.php
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
 * @param array<string, string> $widgets
 *
 * @return array<string, string>
 */
function backgrounds(Tk $tk, array $widgets): array
{
    $colors = [];
    foreach ($widgets as $field => $widget) {
        $colors[$field] = $tk->evaluate($widget . ' cget -background');
    }

    return $colors;
}

$entries = [
    're' => Layout::ENTRY_RE,
    'im' => Layout::ENTRY_IM,
    'depth' => Layout::ENTRY_DEPTH,
];

$tk = new Tk();
$tk->open(WINDOW_WIDTH, WINDOW_HEIGHT);
$tk->pump();
check($tk->pump() === [], 'event queue starts empty');
$tk->setTitle('Swow Mandelbrot Auto Zoom');
check($tk->evaluate('wm title .') === 'Swow Mandelbrot Auto Zoom', 'setTitle changes the window title');
$tk->setTitle(Layout::TITLE);

$defaultBackgrounds = backgrounds($tk, $entries);
$tk->setFields('-0.75', '0.1', '1e5');
check($tk->evaluate(Layout::ENTRY_RE . ' get') === '-0.75', 'setFields syncs the real entry');
check($tk->evaluate(Layout::ENTRY_IM . ' get') === '0.1', 'setFields syncs the imaginary entry');
check($tk->evaluate(Layout::ENTRY_DEPTH . ' get') === '1e5', 'setFields syncs the depth entry');

$commands = [
    Layout::BUTTON_ZOOM_OUT => Layout::CMD_ZOOM_OUT,
    Layout::BUTTON_ZOOM_IN => Layout::CMD_ZOOM_IN,
    Layout::BUTTON_RESET => Layout::CMD_RESET,
    Layout::BUTTON_GO => Layout::CMD_APPLY,
    Layout::BUTTON_SAVE => Layout::CMD_SAVE,
    Layout::BUTTON_QUIT => Layout::CMD_QUIT,
    Layout::BUTTON_PAN_UP => Layout::CMD_PAN_UP,
    Layout::BUTTON_PAN_DOWN => Layout::CMD_PAN_DOWN,
    Layout::BUTTON_PAN_LEFT => Layout::CMD_PAN_LEFT,
    Layout::BUTTON_PAN_RIGHT => Layout::CMD_PAN_RIGHT,
];
foreach ($commands as $button => $command) {
    check(
        $tk->evaluate($button . ' cget -command') === $command,
        $button . ' is bound to ' . $command,
    );
}
foreach ($entries as $field => $widget) {
    check(
        $tk->evaluate('bind ' . $widget . ' <Return>') === Layout::CMD_APPLY,
        'Return on the ' . $field . ' entry is bound to apply',
    );
}

$busyWidgets = [
    Layout::ENTRY_RE,
    Layout::ENTRY_IM,
    Layout::ENTRY_DEPTH,
    Layout::BUTTON_ZOOM_OUT,
    Layout::BUTTON_ZOOM_IN,
    Layout::BUTTON_RESET,
    Layout::BUTTON_GO,
    Layout::BUTTON_SAVE,
    Layout::BUTTON_PAN_UP,
    Layout::BUTTON_PAN_DOWN,
    Layout::BUTTON_PAN_LEFT,
    Layout::BUTTON_PAN_RIGHT,
];
$tk->status('busy probe');
$tk->setBusy(true);
foreach ($busyWidgets as $widget) {
    check($tk->evaluate($widget . ' cget -state') === 'disabled', $widget . ' is disabled while busy');
}
check($tk->evaluate(Layout::BUTTON_QUIT . ' cget -state') === 'normal', 'quit stays enabled while busy');
check(
    $tk->evaluate(Layout::STATUS . ' cget -text') === 'busy probe' &&
        (int) $tk->evaluate('winfo ismapped ' . Layout::STATUS) === 1,
    'status label stays readable while busy',
);
$tk->setBusy(true);
foreach ($busyWidgets as $widget) {
    check($tk->evaluate($widget . ' cget -state') === 'disabled', $widget . ' is still disabled after a second setBusy(true)');
}
check($tk->evaluate(Layout::BUTTON_QUIT . ' cget -state') === 'normal', 'quit stays enabled after a second setBusy(true)');

$tk->setBusy(false);
foreach ($busyWidgets as $widget) {
    check($tk->evaluate($widget . ' cget -state') === 'normal', $widget . ' is enabled after setBusy(false)');
}
check($tk->evaluate(Layout::BUTTON_QUIT . ' cget -state') === 'normal', 'quit stays normal after setBusy(false)');
check(
    $tk->evaluate(Layout::STATUS . ' cget -text') === 'busy probe',
    'status text survives the busy toggles',
);
$tk->setBusy(false);
foreach ($busyWidgets as $widget) {
    check($tk->evaluate($widget . ' cget -state') === 'normal', $widget . ' is still enabled after a second setBusy(false)');
}
$tk->status('');

$tk->setBusy(true);
$tk->setFields('1.5', '2.5', '3.5');
check($tk->evaluate(Layout::ENTRY_RE . ' get') === '1.5', 'setFields updates the real entry while busy');
check($tk->evaluate(Layout::ENTRY_IM . ' get') === '2.5', 'setFields updates the imaginary entry while busy');
check($tk->evaluate(Layout::ENTRY_DEPTH . ' get') === '3.5', 'setFields updates the depth entry while busy');
foreach ($entries as $field => $widget) {
    check(
        $tk->evaluate($widget . ' cget -state') === 'disabled',
        $field . ' entry is still disabled after setFields',
    );
}
$tk->setFields('1.6', '2.6', '3.6');
check($tk->evaluate(Layout::ENTRY_RE . ' get') === '1.6', 'a second setFields while busy updates the real entry');
check($tk->evaluate(Layout::ENTRY_IM . ' get') === '2.6', 'a second setFields while busy updates the imaginary entry');
check($tk->evaluate(Layout::ENTRY_DEPTH . ' get') === '3.6', 'a second setFields while busy updates the depth entry');
foreach ($entries as $field => $widget) {
    check(
        $tk->evaluate($widget . ' cget -state') === 'disabled',
        $field . ' entry is still disabled after a second setFields',
    );
}
$tk->setBusy(false);
foreach ($entries as $field => $widget) {
    check(
        $tk->evaluate($widget . ' cget -state') === 'normal',
        $field . ' entry is enabled after setBusy(false)',
    );
}
check($tk->evaluate(Layout::ENTRY_RE . ' get') === '1.6', 'real entry value survives the busy update');
check($tk->evaluate(Layout::ENTRY_IM . ' get') === '2.6', 'imaginary entry value survives the busy update');
check($tk->evaluate(Layout::ENTRY_DEPTH . ' get') === '3.6', 'depth entry value survives the busy update');
$tk->setFields('-0.75', '0.1', '1e5');

$tk->pump();
$waitStart = hrtime(true);
$tk->wait(1000);
$waitMs = (hrtime(true) - $waitStart) / 1e6;
$tk->pump();
$aroundMs = (hrtime(true) - $waitStart) / 1e6;
check($waitMs >= 0.9, 'wait(1000) sleeps at least about 1 ms');
check($aroundMs < 50.0, 'pumps still run around wait(1000)');

$tk->evaluate(Layout::BUTTON_ZOOM_OUT . ' invoke');
$tk->evaluate(Layout::BUTTON_ZOOM_IN . ' invoke');
$tk->evaluate(Layout::BUTTON_RESET . ' invoke');
$tk->evaluate(Layout::BUTTON_PAN_UP . ' invoke');
$tk->evaluate(Layout::BUTTON_PAN_DOWN . ' invoke');
$tk->evaluate(Layout::BUTTON_PAN_LEFT . ' invoke');
$tk->evaluate(Layout::BUTTON_PAN_RIGHT . ' invoke');
$tk->evaluate('focus -force ' . Layout::ENTRY_RE);
$tk->evaluate('event generate ' . Layout::ENTRY_RE . ' <Return>');
$tk->evaluate(Layout::BUTTON_GO . ' invoke');
$tk->evaluate(Layout::BUTTON_SAVE . ' invoke');
$tk->evaluate(Layout::BUTTON_QUIT . ' invoke');

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
    $apply,
    ['type' => 'save'],
    ['type' => 'quit'],
];
$events = $tk->pump();
check($events === $expected, 'buttons and Return queue events in order');
check($tk->pump() === [], 'pump drains the event queue');

foreach ($entries as $field => $widget) {
    $tk->evaluate('focus -force ' . $widget);
    $tk->evaluate('event generate ' . $widget . ' <Return>');
}
$events = $tk->pump();
check(
    $events === [$apply, $apply, $apply],
    'Return on each entry queues an apply event',
);

$tk->evaluate('focus -force ' . Layout::ENTRY_IM);
$tk->evaluate('event generate ' . Layout::ENTRY_IM . ' <Return> -when tail');
check(
    $tk->pump() === [$apply],
    'a queued Return event is handled by pump',
);

foreach ($entries as $field => $widget) {
    $tk->markInvalid($field, 'bad ' . $field);
    check(
        $tk->evaluate($widget . ' cget -background') === '#ff6666',
        'markInvalid paints the ' . $field . ' entry red',
    );
    $other = $defaultBackgrounds;
    unset($other[$field]);
    check(
        array_intersect_key(backgrounds($tk, $entries), $other) === $other,
        'markInvalid leaves the other entries alone',
    );
    check(
        $tk->evaluate(Layout::STATUS . ' cget -text') === 'bad ' . $field,
        'markInvalid writes the ' . $field . ' reason to the status line',
    );

    $tk->markInvalid(null, '');
    check(
        backgrounds($tk, $entries) === $defaultBackgrounds,
        'markInvalid(null) clears the ' . $field . ' mark',
    );
}

$tk->evaluate('destroy ' . Layout::ENTRY_RE);
try {
    $tk->evaluate(Layout::BUTTON_GO . ' invoke');
    fail('apply with a destroyed entry did not fail');
} catch (RuntimeException) {
    echo "ok - callback errors are returned through the Tcl result\n";
}

$tk->close();
echo "PASS: Tk event test\n";
