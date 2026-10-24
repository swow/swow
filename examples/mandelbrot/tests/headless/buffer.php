<?php

declare(strict_types=1);

use Swow\Mandelbrot\Buffer;

require __DIR__ . '/../../src/bootstrap.php';
require __DIR__ . '/../support.php';

$width = 160;
$height = 96;
$maxLimit = 64;
$limit = 32;
$step = 4;

$buffer = Buffer::create($width, $height, $maxLimit);

check($buffer->fullWidth() === $width, 'fullWidth');
check($buffer->fullHeight() === $height, 'fullHeight');
check($buffer->previewWidth() === intdiv($width, $step), 'previewWidth');
check($buffer->previewHeight() === intdiv($height, $step), 'previewHeight');
check($buffer->previewStep() === $step, 'previewStep');
check(
    $buffer->bandCount() === intdiv($height + Buffer::BAND_ROWS - 1, Buffer::BAND_ROWS),
    'bandCount',
);
check($buffer->glitchSlotsPerBand() === Buffer::GLITCH_SLOTS, 'glitchSlotsPerBand');

$header = $buffer->readHeader();
check($header['magic'] === Buffer::MAGIC, 'header magic');
check($header['layoutVersion'] === Buffer::LAYOUT_VERSION, 'header layoutVersion');
check($header['fullWidth'] === $width, 'header fullWidth');
check($header['fullHeight'] === $height, 'header fullHeight');
check($header['limit'] === $maxLimit, 'header limit starts at maxLimit');
check($header['refVersion'] === 0, 'header refVersion');
check($header['frame'] === 0, 'header frame');
check($header['flags'] === 0, 'header flags');
check($header['previewWidth'] === intdiv($width, $step), 'header previewWidth');
check($header['previewHeight'] === intdiv($height, $step), 'header previewHeight');
check($header['glitchTableOffset'] > 0, 'header glitchTableOffset');
check($header['glitchSlotsPerBand'] === Buffer::GLITCH_SLOTS, 'header glitchSlotsPerBand');

$buffer->writeHeader(['limit' => $limit, 'refVersion' => 9, 'frame' => 42, 'flags' => 1]);
$header = $buffer->readHeader();
check($header['limit'] === $limit, 'written limit');
check($header['refVersion'] === 9, 'written refVersion');
check($header['frame'] === 42, 'written frame');
check($header['flags'] === 1, 'written flags');
check($header['magic'] === Buffer::MAGIC, 'partial header write keeps magic');
check($header['previewWidth'] === intdiv($width, $step), 'partial header write keeps previewWidth');

$row = [];
for ($x = 0; $x < $width; $x++) {
    $row[] = ((($x * 37) & 0xFF) << 16) | ((($x * 11) & 0xFF) << 8) | (($x * 3) & 0xFF);
}

$buffer->writeRows(false, 5, $row);
$bytes = $buffer->readRows(false, 5, 6);
check(strlen($bytes) === $width * 4, 'full row byte length');
for ($x = 0; $x < $width; $x++) {
    $word = unpack('V', substr($bytes, $x * 4, 4))[1];
    check($word === (0xFF000000 | $row[$x]), "full row pixel {$x}");
}

$previewRow = array_fill(0, $buffer->previewWidth(), 0x102030);
$buffer->writeRows(true, 1, $previewRow);
$bytes = $buffer->readRows(true, 1, 2);
check(strlen($bytes) === $buffer->previewWidth() * 4, 'preview row byte length');
check(
    ord($bytes[0]) === 0x30 && ord($bytes[1]) === 0x20 && ord($bytes[2]) === 0x10 && ord($bytes[3]) === 0xFF,
    'preview pixel byte order is BGRA',
);

$bandRow = array_fill(0, $width, 0x445566);
$buffer->writeBand(false, 10, array_merge($bandRow, $bandRow));
$bytes = $buffer->readRows(false, 10, 12);
check(strlen($bytes) === 2 * $width * 4, 'band byte length');
for ($x = 0; $x < $width; $x++) {
    check(unpack('V', substr($bytes, $x * 4, 4))[1] === 0xFF445566, "band pixel {$x}");
    check(unpack('V', substr($bytes, ($width + $x) * 4, 4))[1] === 0xFF445566, "band pixel {$x} of row 11");
}

$buffer->writePixel(false, 3, 20, 0xABCDEF);
$bytes = $buffer->readRows(false, 20, 21);
check(unpack('V', substr($bytes, 3 * 4, 4))[1] === 0xFFABCDEF, 'writePixel');
check(unpack('V', substr($bytes, 4 * 4, 4))[1] === 0, 'writePixel leaves neighbours alone');

$buffer->writeGlitches(3, [11, 12, 13, 14]);
$glitches = $buffer->readGlitches(3);
check($glitches['count'] === 2, 'glitch count');
check($glitches['glitches'] === [11, 12, 13, 14], 'glitch coordinates');

$many = [];
for ($i = 0; $i < 3000; $i++) {
    $many[] = $i;
}
$stored = $buffer->writeGlitches(4, $many);
check($stored === Buffer::GLITCH_SLOTS, 'glitch slots cap');
$glitches = $buffer->readGlitches(4);
check($glitches['count'] === Buffer::GLITCH_SLOTS, 'stored glitch count');
check(
    $glitches['glitches'] === array_slice($many, 0, Buffer::GLITCH_SLOTS * 2),
    'stored glitch coordinates stop at the cap',
);

try {
    $buffer->readGlitches($buffer->bandCount());
    fail('band index past bandCount must be rejected');
} catch (InvalidArgumentException) {
}

$bytes = $buffer->readRows(false, 5, 6);
check(
    unpack('V', substr($bytes, 0, 4))[1] === (0xFF000000 | $row[0]),
    'preview write leaves the full frame alone',
);

check(strlen($buffer->readAll(false)) === $width * $height * 4, 'readAll full length');
check(
    strlen($buffer->readAll(true)) === intdiv($width, $step) * intdiv($height, $step) * 4,
    'readAll preview length',
);

$zr = [];
$zi = [];
$mag = [];
for ($n = 0; $n <= $limit; $n++) {
    $zr[] = $n * 0.125 - 0.75;
    $zi[] = sin($n * 0.25) * 0.5;
    $mag[] = $zr[$n] * $zr[$n] + $zi[$n] * $zi[$n];
}

$buffer->writeOrbit($zr, $zi, $mag);
$orbit = $buffer->readOrbit();
check($orbit['zr'] === $zr, 'orbit real part round trip');
check($orbit['zi'] === $zi, 'orbit imaginary part round trip');
check($orbit['mag'] === $mag, 'orbit magnitude round trip');

try {
    $buffer->writeOrbit(array_fill(0, $limit + 2, 0.0), [], []);
    fail('orbit larger than the limit must be rejected');
} catch (InvalidArgumentException) {
}

try {
    $buffer->writeHeader(['bogus' => 1]);
    fail('unknown header field must be rejected');
} catch (InvalidArgumentException) {
}

try {
    $buffer->writeHeader(['limit' => $maxLimit + 1]);
    fail('limit above maxLimit must be rejected');
} catch (InvalidArgumentException) {
}

try {
    $buffer->readRows(false, 5, $height + 1);
    fail('row range past the height must be rejected');
} catch (InvalidArgumentException) {
}

$shared = unserialize($buffer->serializeHandle());
check($shared instanceof Swow\SharedBuffer, 'serializeHandle yields a SharedBuffer');
check($shared->read(0, 4) === "\x4C\x44\x4E\x4D", 'magic is stored little endian');
check($shared->read(8, 4) === pack('V', $width), 'serialized handle points at the same memory');

finish('buffer');
