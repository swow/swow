<?php

declare(strict_types=1);

namespace Swow\Http\Protocol;

use function ord;
use function strlen;

use const PHP_INT_MAX;

final class H2HpackInteger
{
    /**
     * @return array{int, int}
     */
    public static function decode(string $data, int $offset, int $prefixBits, ?int $length = null): array
    {
        $length ??= strlen($data);
        if ($offset >= $length) {
            throw H2Exception::forConnectionError('HPACK integer is truncated');
        }

        $maxPrefix = (1 << $prefixBits) - 1;
        $value = ord($data[$offset]) & $maxPrefix;
        $offset++;

        if ($value < $maxPrefix) {
            return [$value, $offset];
        }

        $shift = 0;
        do {
            if ($offset >= $length) {
                throw H2Exception::forConnectionError('HPACK integer is truncated');
            }

            $byte = ord($data[$offset]);
            $offset++;

            if ($shift > 56) {
                throw H2Exception::forConnectionError('HPACK integer overflow');
            }

            $increment = ($byte & 0x7F) << $shift;
            if ($value > (PHP_INT_MAX - $increment)) {
                throw H2Exception::forConnectionError('HPACK integer overflow');
            }

            $value += $increment;
            $shift += 7;
        } while (($byte & 0x80) !== 0);

        return [$value, $offset];
    }
}
