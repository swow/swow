<?php

declare(strict_types=1);

namespace Swow\Mandelbrot\Gui\Win32;

use FFI;
use FFI\CData;

/**
 * UTF-16LE conversion and window text access for the Win32 backend.
 *
 * One instance owns the scratch buffer that windowText() reads into, so reads
 * do not allocate per call.
 */
final class Text
{
    private const BUFFER_CHARS = 32768;

    private ?CData $buffer = null;

    public function __construct(private readonly FFI $user32)
    {
        $this->buffer = $user32->new('uint16_t[' . self::BUFFER_CHARS . ']');
    }

    /**
     * Encode UTF-8 as UTF-16LE without a terminator.
     *
     * Invalid sequences become U+FFFD so window text never fails to convert.
     */
    public static function toUtf16Le(string $text): string
    {
        $out = '';
        $length = strlen($text);

        for ($i = 0; $i < $length;) {
            $byte = ord($text[$i]);
            if ($byte < 0x80) {
                $codePoint = $byte;
                $continuation = 0;
                $i++;
            } elseif (($byte & 0xE0) === 0xC0) {
                $codePoint = $byte & 0x1F;
                $continuation = 1;
                $i++;
            } elseif (($byte & 0xF0) === 0xE0) {
                $codePoint = $byte & 0x0F;
                $continuation = 2;
                $i++;
            } elseif (($byte & 0xF8) === 0xF0) {
                $codePoint = $byte & 0x07;
                $continuation = 3;
                $i++;
            } else {
                $out .= "\xFD\xFF";
                $i++;
                continue;
            }

            $valid = true;
            for ($j = 0; $j < $continuation; $j++) {
                if ($i >= $length || (ord($text[$i]) & 0xC0) !== 0x80) {
                    $valid = false;
                    break;
                }
                $codePoint = ($codePoint << 6) | (ord($text[$i]) & 0x3F);
                $i++;
            }

            if (!$valid
                || $codePoint > 0x10FFFF
                || ($codePoint >= 0xD800 && $codePoint <= 0xDFFF)
            ) {
                $out .= "\xFD\xFF";
                continue;
            }

            if ($codePoint >= 0x10000) {
                $codePoint -= 0x10000;
                $out .= pack(
                    'vv',
                    0xD800 | ($codePoint >> 10),
                    0xDC00 | ($codePoint & 0x3FF),
                );
            } else {
                $out .= pack('v', $codePoint);
            }
        }

        return $out;
    }

    /**
     * Decode UTF-16LE to UTF-8, stopping at the first NUL code unit.
     */
    public static function fromUtf16Le(string $bytes): string
    {
        $out = '';
        $length = strlen($bytes);
        $length -= $length % 2;

        for ($i = 0; $i < $length; $i += 2) {
            $unit = ord($bytes[$i]) | (ord($bytes[$i + 1]) << 8);
            if ($unit === 0) {
                break;
            }

            if ($unit >= 0xD800 && $unit <= 0xDBFF) {
                $low = 0;
                if ($i + 3 < $length) {
                    $low = ord($bytes[$i + 2]) | (ord($bytes[$i + 3]) << 8);
                }
                if ($low >= 0xDC00 && $low <= 0xDFFF) {
                    $codePoint = 0x10000 + (($unit - 0xD800) << 10) + ($low - 0xDC00);
                    $i += 2;
                } else {
                    $codePoint = 0xFFFD;
                }
            } elseif ($unit >= 0xDC00 && $unit <= 0xDFFF) {
                $codePoint = 0xFFFD;
            } else {
                $codePoint = $unit;
            }

            $out .= self::utf8FromCodePoint($codePoint);
        }

        return $out;
    }

    /**
     * Allocate a NUL-terminated UTF-16LE buffer for one of the FFI modules.
     */
    public static function wideFor(FFI $ffi, string $text): CData
    {
        $bytes = self::toUtf16Le($text) . "\0\0";
        $buffer = $ffi->new('uint16_t[' . intdiv(strlen($bytes), 2) . ']');
        FFI::memcpy($buffer, $bytes, strlen($bytes));

        return $buffer;
    }

    public function wide(string $text): CData
    {
        return self::wideFor($this->user32, $text);
    }

    public function setWindowText(int $handle, string $text): void
    {
        if ($handle !== 0) {
            $this->user32->SetWindowTextW($handle, $this->wide($text));
        }
    }

    public function windowText(int $handle): string
    {
        if ($handle === 0) {
            return '';
        }

        $length = $this->user32->GetWindowTextW(
            $handle,
            $this->buffer,
            self::BUFFER_CHARS,
        );
        if ($length <= 0) {
            return '';
        }

        return self::fromUtf16Le(FFI::string($this->buffer, $length * 2));
    }

    private static function utf8FromCodePoint(int $codePoint): string
    {
        if ($codePoint < 0x80) {
            return chr($codePoint);
        }
        if ($codePoint < 0x800) {
            return chr(0xC0 | ($codePoint >> 6))
                . chr(0x80 | ($codePoint & 0x3F));
        }
        if ($codePoint < 0x10000) {
            return chr(0xE0 | ($codePoint >> 12))
                . chr(0x80 | (($codePoint >> 6) & 0x3F))
                . chr(0x80 | ($codePoint & 0x3F));
        }

        return chr(0xF0 | ($codePoint >> 18))
            . chr(0x80 | (($codePoint >> 12) & 0x3F))
            . chr(0x80 | (($codePoint >> 6) & 0x3F))
            . chr(0x80 | ($codePoint & 0x3F));
    }
}
