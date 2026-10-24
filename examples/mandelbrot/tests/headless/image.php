<?php

declare(strict_types=1);

use Swow\Mandelbrot\Image;

require __DIR__ . '/../../src/bootstrap.php';
require __DIR__ . '/../support.php';

$width = 64;
$height = 48;

$bgra = '';
for ($y = 0; $y < $height; $y++) {
    for ($x = 0; $x < $width; $x++) {
        $bgra .= chr(($x * 3) & 0xFF) . chr(($y * 5) & 0xFF) . chr(0x40) . chr(0xFF);
    }
}

try {
    Image::encode($bgra, $width + 1, $height);
    fail('mismatched BGRA size must be rejected');
} catch (InvalidArgumentException) {
}

$path = tempPath('image', '.png');
Image::save($path, $bgra, $width, $height);

$info = getimagesize($path);
check($info !== false, 'getimagesize reads the file');
check($info[0] === $width, 'PNG width');
check($info[1] === $height, 'PNG height');
check($info['mime'] === 'image/png', 'PNG mime type');

$png = file_get_contents($path);
check($png !== false && $png !== '', 'PNG file is not empty');
check(substr($png, 0, 8) === "\x89PNG\r\n\x1a\n", 'PNG signature');

$chunks = [];
$idat = '';
$offset = 8;
$length = strlen($png);
while ($offset < $length) {
    $chunkLength = unpack('N', substr($png, $offset, 4))[1];
    $type = substr($png, $offset + 4, 4);
    $data = substr($png, $offset + 8, $chunkLength);
    $crc = unpack('N', substr($png, $offset + 8 + $chunkLength, 4))[1];
    check($crc === crc32($type . $data), "CRC of chunk {$type}");
    $chunks[] = $type;
    if ($type === 'IDAT') {
        $idat .= $data;
    }
    $offset += 12 + $chunkLength;
}

check($chunks === ['IHDR', 'IDAT', 'IEND'], 'chunk order');

$ihdr = unpack(
    'Nwidth/Nheight/CbitDepth/CcolorType/Ccompression/Cfilter/Cinterlace',
    substr($png, 16, 13),
);
check($ihdr['width'] === $width, 'IHDR width');
check($ihdr['height'] === $height, 'IHDR height');
check($ihdr['bitDepth'] === 8, 'IHDR bit depth');
check($ihdr['colorType'] === 6, 'IHDR color type is RGBA');
check($ihdr['compression'] === 0 && $ihdr['filter'] === 0 && $ihdr['interlace'] === 0, 'IHDR defaults');

$raw = gzuncompress($idat);
check($raw !== false, 'IDAT decompresses');
$rowBytes = 1 + $width * 4;
check(strlen($raw) === $height * $rowBytes, 'raw scanline length');

for ($y = 0; $y < $height; $y++) {
    $base = $y * $rowBytes;
    check($raw[$base] === "\x00", "filter byte of row {$y}");
    for ($x = 0; $x < $width; $x++) {
        $target = $base + 1 + $x * 4;
        check($raw[$target] === chr(0x40), "red channel of pixel {$x},{$y}");
        check($raw[$target + 1] === chr(($y * 5) & 0xFF), "green channel of pixel {$x},{$y}");
        check($raw[$target + 2] === chr(($x * 3) & 0xFF), "blue channel of pixel {$x},{$y}");
        check($raw[$target + 3] === chr(0xFF), "alpha channel of pixel {$x},{$y}");
    }
}

removeTempFiles($path);
finish('image');
