<?php

declare(strict_types=1);

namespace Swow\Mandelbrot\Gui\Win32;

use InvalidArgumentException;
use RuntimeException;

use function chr;
use function ord;
use function sprintf;
use function strlen;

/**
 * Minimal PNG decoder for the bundled placeholder image.
 *
 * Supports 8-bit, non-interlaced images of color types 0, 2, 3, 4 and 6 with
 * filter types 0 to 4. Chunk CRCs are not verified because this decoder only
 * reads the bundled placeholder. Color types without alpha yield opaque BGRA.
 */
final class Png
{
    private const MAX_DIMENSION = 16384;

    private const SIGNATURE = "\x89PNG\r\n\x1a\n";
    private const CHANNELS = [
        0 => 1,
        2 => 3,
        3 => 1,
        4 => 2,
        6 => 4,
    ];

    /**
     * Decode an 8-bit, non-interlaced PNG into top-down BGRA pixels.
     *
     * @return array{width: int, height: int, bgra: string}
     */
    public static function decode(string $data): array
    {
        if (!str_starts_with($data, self::SIGNATURE)) {
            throw new InvalidArgumentException('not a PNG file');
        }

        $offset = strlen(self::SIGNATURE);
        $total = strlen($data);
        $width = 0;
        $height = 0;
        $colorType = -1;
        $palette = '';
        $transparency = '';
        $idat = '';
        $seenHeader = false;

        while ($offset + 8 <= $total) {
            $length = unpack('N', substr($data, $offset, 4))[1];
            $type = substr($data, $offset + 4, 4);
            if ($offset + 12 + $length > $total) {
                throw new InvalidArgumentException('truncated PNG chunk: ' . $type);
            }
            $chunk = substr($data, $offset + 8, $length);
            $offset += 12 + $length;

            if ($type === 'IHDR') {
                if ($length !== 13) {
                    throw new InvalidArgumentException('invalid PNG header length');
                }
                $header = unpack(
                    'Nwidth/Nheight/CbitDepth/CcolorType/Ccompression/Cfilter/Cinterlace',
                    $chunk,
                );
                $width = $header['width'];
                $height = $header['height'];
                if ($width < 1 ||
                    $height < 1 ||
                    $width > self::MAX_DIMENSION ||
                    $height > self::MAX_DIMENSION
                ) {
                    throw new InvalidArgumentException('unsupported PNG dimensions');
                }
                if ($header['bitDepth'] !== 8) {
                    throw new InvalidArgumentException('only 8-bit PNG is supported');
                }
                if ($header['compression'] !== 0 || $header['filter'] !== 0) {
                    throw new InvalidArgumentException('unsupported PNG compression or filter method');
                }
                if ($header['interlace'] !== 0) {
                    throw new InvalidArgumentException('interlaced PNG is not supported');
                }
                $colorType = $header['colorType'];
                if (!isset(self::CHANNELS[$colorType])) {
                    throw new InvalidArgumentException('unsupported PNG color type ' . $colorType);
                }
                $seenHeader = true;
                continue;
            }

            if (!$seenHeader) {
                throw new InvalidArgumentException('PNG chunk before IHDR');
            }
            if ($type === 'PLTE') {
                $palette = $chunk;
            } elseif ($type === 'tRNS') {
                $transparency = $chunk;
            } elseif ($type === 'IDAT') {
                $idat .= $chunk;
            } elseif ($type === 'IEND') {
                break;
            }
        }

        if (!$seenHeader || $idat === '') {
            throw new InvalidArgumentException('PNG has no image data');
        }
        if ($colorType === 3 && strlen($palette) < 3) {
            throw new InvalidArgumentException('indexed PNG has no palette');
        }

        $channels = self::CHANNELS[$colorType];
        $rowBytes = $width * $channels;
        $expected = $height * ($rowBytes + 1);
        $raw = zlib_decode($idat, $expected);
        if ($raw === false) {
            throw new RuntimeException('cannot inflate PNG image data');
        }
        if (strlen($raw) !== $expected) {
            throw new RuntimeException(sprintf(
                'PNG image data is %d bytes, expected %d',
                strlen($raw),
                $expected,
            ));
        }

        $bgra = '';
        $previous = str_repeat("\0", $rowBytes);
        $position = 0;

        for ($y = 0; $y < $height; $y++) {
            $filter = ord($raw[$position]);
            $position++;
            $row = substr($raw, $position, $rowBytes);
            $position += $rowBytes;

            for ($i = 0; $i < $rowBytes; $i++) {
                $value = ord($row[$i]);
                $left = $i >= $channels ? ord($row[$i - $channels]) : 0;
                $up = ord($previous[$i]);
                $upLeft = $i >= $channels ? ord($previous[$i - $channels]) : 0;

                switch ($filter) {
                    case 0:
                        break;
                    case 1:
                        $value = ($value + $left) & 0xFF;
                        break;
                    case 2:
                        $value = ($value + $up) & 0xFF;
                        break;
                    case 3:
                        $value = ($value + (($left + $up) >> 1)) & 0xFF;
                        break;
                    case 4:
                        $value = ($value + self::paeth($left, $up, $upLeft)) & 0xFF;
                        break;
                    default:
                        throw new RuntimeException('unknown PNG filter ' . $filter);
                }

                $row[$i] = chr($value);
            }

            $bgra .= self::rowToBgra($row, $width, $colorType, $palette, $transparency);
            $previous = $row;
        }

        return [
            'width' => $width,
            'height' => $height,
            'bgra' => $bgra,
        ];
    }

    private static function paeth(int $left, int $up, int $upLeft): int
    {
        $estimate = $left + $up - $upLeft;
        $leftDistance = abs($estimate - $left);
        $upDistance = abs($estimate - $up);
        $upLeftDistance = abs($estimate - $upLeft);

        if ($leftDistance <= $upDistance && $leftDistance <= $upLeftDistance) {
            return $left;
        }

        return $upDistance <= $upLeftDistance ? $up : $upLeft;
    }

    private static function rowToBgra(
        string $row,
        int $width,
        int $colorType,
        string $palette,
        string $transparency,
    ): string {
        $out = '';

        switch ($colorType) {
            case 0:
                for ($x = 0; $x < $width; $x++) {
                    $gray = $row[$x];
                    $out .= $gray . $gray . $gray . "\xFF";
                }
                break;

            case 2:
                for ($x = 0; $x < $width; $x++) {
                    $offset = $x * 3;
                    $out .= $row[$offset + 2] . $row[$offset + 1] . $row[$offset] . "\xFF";
                }
                break;

            case 3:
                $paletteSize = intdiv(strlen($palette), 3);
                for ($x = 0; $x < $width; $x++) {
                    $index = ord($row[$x]);
                    if ($index >= $paletteSize) {
                        throw new RuntimeException('PNG palette index out of range');
                    }
                    $offset = $index * 3;
                    $alpha = $index < strlen($transparency) ? $transparency[$index] : "\xFF";
                    $out .= $palette[$offset + 2]
                        . $palette[$offset + 1]
                        . $palette[$offset]
                        . $alpha;
                }
                break;

            case 4:
                for ($x = 0; $x < $width; $x++) {
                    $offset = $x * 2;
                    $gray = $row[$offset];
                    $out .= $gray . $gray . $gray . $row[$offset + 1];
                }
                break;

            default:
                for ($x = 0; $x < $width; $x++) {
                    $offset = $x * 4;
                    $out .= $row[$offset + 2]
                        . $row[$offset + 1]
                        . $row[$offset]
                        . $row[$offset + 3];
                }
        }

        return $out;
    }
}
