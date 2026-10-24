<?php

declare(strict_types=1);

/*
 * Deep zoom smoke test with the boundary coordinate from the numeric tests.
 *
 * The default centre leaves the set, so past about 1e15 it renders flat. This
 * coordinate stays within one pixel of the boundary up to 1e30.
 */

use Swow\Mandelbrot\App;
use Swow\Mandelbrot\Config;
use Swow\Mandelbrot\Palette;

require __DIR__ . '/../../src/bootstrap.php';
require __DIR__ . '/../support.php';

/**
 * @return array{int, int, string} width, height and BGRA bytes
 */
function decodeBgraPng(string $path): array
{
    $png = file_get_contents($path);
    check($png !== false && substr($png, 0, 8) === "\x89PNG\r\n\x1a\n", 'output is a PNG');

    $offset = 8;
    $length = strlen($png);
    $width = 0;
    $height = 0;
    $idat = '';
    while ($offset < $length) {
        $chunkLength = unpack('N', substr($png, $offset, 4))[1];
        $type = substr($png, $offset + 4, 4);
        $data = substr($png, $offset + 8, $chunkLength);
        if ($type === 'IHDR') {
            $header = unpack('Nwidth/Nheight', $data);
            $width = $header['width'];
            $height = $header['height'];
        } elseif ($type === 'IDAT') {
            $idat .= $data;
        }
        $offset += 12 + $chunkLength;
    }

    $raw = gzuncompress($idat);
    check($raw !== false, 'IDAT decompresses');
    $rowBytes = 1 + $width * 4;
    check(strlen($raw) === $height * $rowBytes, 'scanline count');

    $filtersOk = true;
    $bgra = '';
    for ($y = 0; $y < $height; $y++) {
        $base = $y * $rowBytes;
        if ($raw[$base] !== "\x00") {
            $filtersOk = false;
            break;
        }
        for ($x = 0; $x < $width; $x++) {
            $source = $base + 1 + $x * 4;
            $bgra .= $raw[$source + 2] . $raw[$source + 1] . $raw[$source] . $raw[$source + 3];
        }
    }
    check($filtersOk, 'scanline filter bytes are zero');

    return [$width, $height, $bgra];
}

/**
 * @return array{seconds: float, inside: int, colours: int, digest: string, png: string}
 */
function renderDeep(string $depth): array
{
    $width = 320;
    $height = 180;
    $output = tempPath('deep-' . $depth, '.png');
    $config = Config::fromArgv([
        'mandelbrot',
        '--headless',
        '--threads=4',
        '--width=' . $width,
        '--height=' . $height,
        '--center=' . DEEP_CENTER_RE . ',' . DEEP_CENTER_IM,
        '--depth=' . $depth,
        '--output=' . $output,
    ]);

    $started = hrtime(true);
    $exit = (new App($config, null))->run();
    $seconds = (hrtime(true) - $started) / 1e9;
    check($exit === 0, "{$depth} exits zero");

    [$decodedWidth, $decodedHeight, $bgra] = decodeBgraPng($output);
    check($decodedWidth === $width && $decodedHeight === $height, "{$depth} image size");
    $pngDigest = hash_file('sha256', $output);
    removeTempFiles($output);

    $colors = [];
    $inside = 0;
    for ($offset = 0; $offset < strlen($bgra); $offset += 4) {
        $color = ord($bgra[$offset + 2]) << 16 | ord($bgra[$offset + 1]) << 8 | ord($bgra[$offset]);
        if ($color === Palette::INSIDE) {
            $inside++;
        }
        $colors[] = $color;
    }

    return [
        'seconds' => $seconds,
        'inside' => $inside,
        'colours' => count(array_unique($colors)),
        'digest' => digestColors($colors),
        'png' => $pngDigest,
    ];
}

foreach (['1e20', '1e30'] as $depth) {
    $result = renderDeep($depth);
    printf(
        "%s: %.2f s, inside %d/%d, colours %d, colour digest %s, png sha256 %s\n",
        $depth,
        $result['seconds'],
        $result['inside'],
        320 * 180,
        $result['colours'],
        $result['digest'],
        $result['png'],
    );
    if ($depth === '1e20') {
        check($result['inside'] > 0, '1e20 has inside pixels');
    }
    // The numeric fixtures record 0 inside pixels for 1e30 at the deep
    // coordinate; structure is judged by the colour count.
    check($result['colours'] > 8, "{$depth} has more than a few colours");
}

finish('deep');
