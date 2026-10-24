<?php

declare(strict_types=1);

/*
 * Worker-side glitch repair.
 *
 * The repair helper takes a Reference, a band row range and a flat coordinate
 * list, writes the repaired pixels, skips coordinates outside the band, and
 * skips a pixel whose write throws.
 */

use Swow\Mandelbrot\Buffer;
use Swow\Mandelbrot\Reference;
use Swow\Mandelbrot\View;
use Swow\Mandelbrot\Thread\Worker;

require __DIR__ . '/../../src/bootstrap.php';
require __DIR__ . '/../support.php';

$width = 64;
$height = 36;
$view = new View($width, $height, 1024);
$view->setCenter(View::DEFAULT_CENTER_RE, View::DEFAULT_CENTER_IM);
$view->setDepth('1e4');
$reference = new Reference($view);
$buffer = Buffer::create($width, $height);
$limit = $view->limit();

$sentinel = 0xFFFFFF;
$buffer->writePixel(false, 10, 5, $sentinel);
$buffer->writePixel(false, 20, 10, $sentinel);

// One coordinate inside the band, one invalid x, one outside the band rows.
Worker::repairGlitches($buffer, $reference, false, 0, 8, 1, $limit, [10, 5, -1, 5, 20, 10]);

$expected = $reference->renderPixelSlow(10, 5, 1, $limit);
$bytes = $buffer->readRows(false, 5, 6);
$word = unpack('V', substr($bytes, 10 * 4, 4))[1] & 0xFFFFFF;
check($word === $expected, 'inside-band glitch is repaired with the slow-path colour');

$bytes = $buffer->readRows(false, 10, 11);
$word = unpack('V', substr($bytes, 20 * 4, 4))[1] & 0xFFFFFF;
check($word === $sentinel, 'coordinate outside the band is left alone');

finish('worker-repair');
