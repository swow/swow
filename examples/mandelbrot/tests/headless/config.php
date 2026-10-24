<?php

declare(strict_types=1);

use Swow\Mandelbrot\Config;

require __DIR__ . '/../../src/bootstrap.php';
require __DIR__ . '/../support.php';

$config = Config::fromArgv(['mandelbrot']);
check(!$config->headless(), 'default headless');
check($config->threads() >= 1, 'default threads');
check($config->width() === 1280, 'default width');
check($config->height() === 720, 'default height');
check(
    $config->centerRe() === '-0.743643887018689113498922249712800000007037228233600378464531',
    'default center real',
);
check(
    $config->centerIm() === '0.131825904211894657975878336475200000004669779776843260798656',
    'default center imaginary',
);
check($config->depth() === 1.0e4, 'default depth');
check($config->depthText() === '1e4', 'default depth text');
check($config->baseIterations() === 1024, 'default baseIterations');
check($config->output() === 'mandelbrot.png', 'default output');
check($config->frames() === 1, 'default frames');
check(!$config->verify(), 'default verify');
check(!$config->auto(), 'default auto');
check($config->autoMs() === 200, 'default auto interval');

$config = Config::fromArgv([
    'mandelbrot',
    '--headless',
    '--threads=3',
    '--width=320',
    '--height=240',
    '--center=0.25,-0.5',
    '--depth=1e6',
    '--iterations=2048',
    '--output=/tmp/mandelbrot.png',
    '--frames=4',
    '--verify',
]);
check($config->headless(), 'parsed headless');
check($config->threads() === 3, 'parsed threads');
check($config->width() === 320, 'parsed width');
check($config->height() === 240, 'parsed height');
check($config->centerRe() === '0.25', 'parsed center real');
check($config->centerIm() === '-0.5', 'parsed center imaginary');
check($config->depth() === 1.0e6, 'parsed depth');
check($config->depthText() === '1e6', 'parsed depth text');
check($config->baseIterations() === 2048, 'parsed baseIterations');
check($config->output() === '/tmp/mandelbrot.png', 'parsed output');
check($config->frames() === 4, 'parsed frames');
check($config->verify(), 'parsed verify');

$config = Config::fromArgv([
    'mandelbrot',
    '--width',
    '640',
    '--depth',
    '1e10',
    '--center',
    '-1,0.5',
]);
check($config->width() === 640, 'separate width value');
check($config->depth() === 1.0e10, 'separate depth value');
check($config->depthText() === '1e10', 'separate depth text');
check($config->centerRe() === '-1', 'separate center value');

$config = Config::fromArgv([
    'mandelbrot',
    '--center=123456789012345678901234567890.1234567890123456789,0.000000000000000000000000000001',
    '--depth=123456789012345.6789',
]);
check(
    $config->centerRe() === '123456789012345678901234567890.1234567890123456789',
    'long center text is preserved',
);
check($config->depthText() === '123456789012345.6789', 'long depth text is preserved');

$config = Config::fromArgv(['mandelbrot', '--auto=50']);
check($config->auto(), 'auto flag');
check($config->autoMs() === 50, 'auto interval value');

$config = Config::fromArgv(['mandelbrot', '--auto']);
check($config->auto(), 'bare auto flag');
check($config->autoMs() === 200, 'bare auto default interval');

$invalid = [
    ['mandelbrot', '--bogus'],
    ['mandelbrot', '--width=abc'],
    ['mandelbrot', '--width=0'],
    ['mandelbrot', '--threads'],
    ['mandelbrot', '--center=1'],
    ['mandelbrot', '--center=1,2,3'],
    ['mandelbrot', '--center=nan,0'],
    ['mandelbrot', '--depth=0'],
    ['mandelbrot', '--depth=-2'],
    ['mandelbrot', '--depth=inf'],
    ['mandelbrot', '--output='],
    ['mandelbrot', '--headless=1'],
    ['mandelbrot', '--iterations=0'],
    ['mandelbrot', '--frames=0'],
    ['mandelbrot', '--auto=0'],
    ['mandelbrot', '--auto=-5'],
    ['mandelbrot', '--auto=abc'],
    ['mandelbrot', '--auto='],
    ['mandelbrot', '--headless', '--auto'],
    ['mandelbrot', 'stray'],
];
foreach ($invalid as $argv) {
    try {
        Config::fromArgv($argv);
        fail('invalid arguments must be rejected: ' . implode(' ', $argv));
    } catch (InvalidArgumentException) {
    }
}

$usage = Config::usage();
check(str_contains($usage, 'Usage: mandelbrot [options]'), 'usage header');
check(str_contains($usage, '--headless'), 'usage lists headless');
check(str_contains($usage, '--auto[=MS]'), 'usage lists auto interval');

try {
    Config::fromArgv(['mandelbrot', '--headless', '--auto']);
    fail('--auto with --headless must be rejected');
} catch (InvalidArgumentException $error) {
    check(str_contains($error->getMessage(), '--auto needs a window'), 'headless auto error message');
}

finish('config');
