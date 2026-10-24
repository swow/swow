<?php

declare(strict_types=1);

use Swow\Mandelbrot\App;
use Swow\Mandelbrot\Config;

require __DIR__ . '/../../src/bootstrap.php';
require __DIR__ . '/../support.php';

$width = 64;
$height = 36;
$base = tempPath('render');

$single = Config::fromArgv([
    'mandelbrot',
    '--headless',
    '--threads=1',
    '--width=' . $width,
    '--height=' . $height,
    '--depth=1e8',
    '--output=' . $base . '-one.png',
]);
check((new App($single, null))->run() === 0, 'single worker run');

$many = Config::fromArgv([
    'mandelbrot',
    '--headless',
    '--threads=3',
    '--width=' . $width,
    '--height=' . $height,
    '--depth=1e8',
    '--output=' . $base . '-many.png',
]);
check((new App($many, null))->run() === 0, 'three worker run');

$singleBytes = file_get_contents($base . '-one.png');
$manyBytes = file_get_contents($base . '-many.png');
check($singleBytes !== false && $manyBytes !== false, 'PNG files exist');
check($singleBytes === $manyBytes, 'worker count does not change the output');

$info = getimagesize($base . '-many.png');
check($info !== false, 'getimagesize reads the output');
check($info[0] === $width && $info[1] === $height, 'output dimensions');
check($info['mime'] === 'image/png', 'output mime type');

removeTempFiles($base . '-one.png', $base . '-many.png');

finish('render');
