<?php

declare(strict_types=1);

/*
 * Pipeline consistency at depth 1e10.
 *
 * App renders through workers into the shared buffer, the reference orbit is
 * written through Buffer, and Renderer is also driven directly in the main
 * process. Any difference here is a buffer or worker plumbing bug, not a
 * numeric one.
 */

use Swow\Futex;
use Swow\Mandelbrot\App;
use Swow\Mandelbrot\Buffer;
use Swow\Mandelbrot\Config;
use Swow\Mandelbrot\Reference;
use Swow\Mandelbrot\Renderer;
use Swow\Mandelbrot\Thread\Worker;
use Swow\Mandelbrot\View;
use Swow\SpscQueue;

require __DIR__ . '/../../src/bootstrap.php';
require __DIR__ . '/../support.php';

/**
 * @return array{int, int, string} width, height and BGRA bytes
 */
function decodePng(string $path): array
{
    $png = file_get_contents($path);
    check($png !== false && substr($png, 0, 8) === "\x89PNG\r\n\x1a\n", 'output is a PNG');

    $offset = 8;
    $length = strlen($png);
    $width = 0;
    $height = 0;
    $idat = '';
    $chunks = [];
    while ($offset < $length) {
        $chunkLength = unpack('N', substr($png, $offset, 4))[1];
        $type = substr($png, $offset + 4, 4);
        $data = substr($png, $offset + 8, $chunkLength);
        $chunks[] = $type;
        if ($type === 'IHDR') {
            $header = unpack('Nwidth/Nheight', $data);
            $width = $header['width'];
            $height = $header['height'];
        } elseif ($type === 'IDAT') {
            $idat .= $data;
        }
        $offset += 12 + $chunkLength;
    }
    check($chunks === ['IHDR', 'IDAT', 'IEND'], 'chunk order');

    $raw = gzuncompress($idat);
    check($raw !== false, 'IDAT decompresses');
    $rowBytes = 1 + $width * 4;
    check(strlen($raw) === $height * $rowBytes, 'scanline count');

    $bgra = '';
    for ($y = 0; $y < $height; $y++) {
        $base = $y * $rowBytes;
        check($raw[$base] === "\x00", "filter byte of row {$y}");
        for ($x = 0; $x < $width; $x++) {
            $source = $base + 1 + $x * 4;
            $bgra .= $raw[$source + 2] . $raw[$source + 1] . $raw[$source] . $raw[$source + 3];
        }
    }

    return [$width, $height, $bgra];
}

/**
 * @param list<int> $colors
 */
function colorsToBgra(array $colors): string
{
    $bytes = '';
    foreach ($colors as $color) {
        $bytes .= chr($color & 0xFF) . chr(($color >> 8) & 0xFF) . chr(($color >> 16) & 0xFF) . "\xFF";
    }

    return $bytes;
}

function describeDiff(string $expected, string $actual, int $width): string
{
    $count = min(strlen($expected), strlen($actual));
    for ($offset = 0; $offset < $count; $offset += 4) {
        if (substr($expected, $offset, 4) !== substr($actual, $offset, 4)) {
            $pixel = intdiv($offset, 4);
            $left = unpack('V', substr($expected, $offset, 4))[1];
            $right = unpack('V', substr($actual, $offset, 4))[1];

            return sprintf(
                'first differs at %d,%d expected 0x%08X got 0x%08X',
                $pixel % $width,
                intdiv($pixel, $width),
                $left,
                $right,
            );
        }
    }

    return 'length ' . strlen($expected) . ' vs ' . strlen($actual);
}

$width = 128;
$height = 72;
$depth = '1e10';
$output = tempPath('consistency', '.png');

$config = Config::fromArgv([
    'mandelbrot',
    '--headless',
    '--threads=1',
    '--width=' . $width,
    '--height=' . $height,
    '--depth=' . $depth,
    '--output=' . $output,
]);
$exit = (new App($config, null))->run();
check($exit === 0, 'App run');

[, , $appBytes] = decodePng($output);
removeTempFiles($output);

$view = new View($width, $height, 1024);
$view->setCenter(View::DEFAULT_CENTER_RE, View::DEFAULT_CENTER_IM);
$view->setDepth($depth);
$reference = new Reference($view);
$version = $reference->build();
$orbit = $reference->orbit();
$params = $view->params($width, $height, 1);

// 1. The orbit that workers read back must be exactly what Reference built.
$buffer = Buffer::create($width, $height);
$buffer->writeHeader(['limit' => $params['limit'], 'refVersion' => $version]);
$buffer->writeOrbit($orbit['zr'], $orbit['zi'], $orbit['mag']);
$sharedOrbit = $buffer->readOrbit();

$orbitDiff = '';
if ($sharedOrbit !== $orbit) {
    if (count($sharedOrbit['zr']) !== count($orbit['zr'])) {
        $orbitDiff = 'length ' . count($sharedOrbit['zr']) . ' vs ' . count($orbit['zr']);
    } else {
        foreach (['zr', 'zi', 'mag'] as $part) {
            foreach ($orbit[$part] as $index => $expected) {
                if ($sharedOrbit[$part][$index] !== $expected) {
                    $orbitDiff = sprintf(
                        '%s[%d] expected %.17g got %.17g',
                        $part,
                        $index,
                        $expected,
                        $sharedOrbit[$part][$index],
                    );
                    break 2;
                }
            }
        }
    }
}
check($orbitDiff === '', 'Buffer orbit round trip: ' . $orbitDiff);

// 2. Main-thread render of every band must match what App wrote through workers.
$mainColors = [];
$rowStep = 8;
for ($row = 0; $row < $height; $row += $rowStep) {
    $end = min($row + $rowStep, $height);
    $band = Renderer::renderBand($orbit, $params, $row, $end);
    $expected = ($end - $row) * $width;
    check(count($band['colors']) === $expected, "band {$row}..{$end} colour count");
    array_push($mainColors, ...$band['colors']);
}
$mainBytes = colorsToBgra($mainColors);
check($mainBytes === $appBytes, 'App output matches main-thread render: ' . describeDiff($mainBytes, $appBytes, $width));

// 3. A worker-rendered band must match the same band rendered in the main process.
$workerBuffer = Buffer::create($width, $height);
$workerBuffer->writeHeader(['limit' => $params['limit'], 'refVersion' => $version]);
$workerBuffer->writeOrbit($orbit['zr'], $orbit['zi'], $orbit['mag']);

$tasks = new SpscQueue(256);
$results = new SpscQueue(256);
$futex = new Futex(1);
$worker = Worker::spawn($workerBuffer, $tasks, $results, $futex);
$bandBegin = 8;
$bandEnd = 24;
$tasks->push([
    'gen' => 1,
    'rowBegin' => $bandBegin,
    'rowEnd' => $bandEnd,
    'refVersion' => $version,
    'preview' => false,
    'params' => $params,
]);
$result = $results->pop(30000);
$tasks->push(['quit' => true]);
$worker->wait(30000);
check($worker->getExitStatus() === 0, 'worker exit status');
check($result['gen'] === 1, 'worker token generation');
check($result['bandIndex'] === intdiv($bandBegin, Buffer::BAND_ROWS), 'worker token band index');
check($result['rowBegin'] === $bandBegin && $result['rowEnd'] === $bandEnd, 'worker result rows');

$band = Renderer::renderBand($orbit, $params, $bandBegin, $bandEnd);
$workerBytes = $workerBuffer->readRows(false, $bandBegin, $bandEnd);
$mainBandBytes = colorsToBgra($band['colors']);
check(
    $workerBytes === $mainBandBytes,
    'worker band matches main-thread band: ' . describeDiff($mainBandBytes, $workerBytes, $width),
);

finish('consistency');
