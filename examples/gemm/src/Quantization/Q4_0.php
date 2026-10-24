<?php

declare(strict_types=1);

namespace Swow\Examples\Gemm\Quantization;

use RuntimeException;

use function array_fill;
use function array_values;
use function intdiv;
use function ord;
use function pack;
use function strlen;
use function substr;
use function unpack;

final class Q4_0
{
    public const BLOCK_SIZE = 32;

    public const BLOCK_BYTES = 18;

    public static function generate(int $blocks, int $seed): string
    {
        $state = $seed;
        $data = '';
        for ($block = 0; $block < $blocks; $block++) {
            $data .= pack('v', 0x2C00);
            for ($index = 0; $index < 16; $index++) {
                $state ^= ($state << 13) & 0xFFFFFFFF;
                $state ^= $state >> 17;
                $state ^= ($state << 5) & 0xFFFFFFFF;
                $state &= 0xFFFFFFFF;
                $low = ($state >> 8) & 0x0F;
                $state ^= ($state << 13) & 0xFFFFFFFF;
                $state ^= $state >> 17;
                $state ^= ($state << 5) & 0xFFFFFFFF;
                $state &= 0xFFFFFFFF;
                $high = ($state >> 8) & 0x0F;
                $data .= pack('C', $low | ($high << 4));
            }
        }

        return $data;
    }

    public static function fp16ToF32(int $half): float
    {
        $sign = ($half & 0x8000) << 16;
        $exponent = ($half >> 10) & 0x1F;
        $mantissa = $half & 0x3FF;

        if ($exponent === 0) {
            if ($mantissa === 0) {
                $bits = $sign;
            } else {
                $exponent = 127 - 15 + 1;
                while (($mantissa & 0x400) === 0) {
                    $mantissa <<= 1;
                    $exponent--;
                }
                $mantissa &= 0x3FF;
                $bits = $sign | ($exponent << 23) | ($mantissa << 13);
            }
        } elseif ($exponent === 31) {
            $bits = $sign | 0x7F800000 | ($mantissa << 13);
        } else {
            $bits = $sign | (($exponent - 15 + 127) << 23) | ($mantissa << 13);
        }

        return unpack('G', pack('N', $bits))[1];
    }

    /** @return list<float> */
    public static function dequantize(string $blocks): array
    {
        $count = intdiv(strlen($blocks), self::BLOCK_BYTES);
        $values = array_fill(0, $count * self::BLOCK_SIZE, 0.0);
        for ($block = 0; $block < $count; $block++) {
            $offset = $block * self::BLOCK_BYTES;
            $scale = self::fp16ToF32(unpack('v', substr($blocks, $offset, 2))[1]);
            $base = $block * self::BLOCK_SIZE;
            for ($index = 0; $index < 16; $index++) {
                $byte = ord($blocks[$offset + 2 + $index]);
                $values[$base + $index] = (($byte & 0x0F) - 8) * $scale;
                $values[$base + $index + 16] = (($byte >> 4) - 8) * $scale;
            }
        }

        return $values;
    }

    /** @param list<float> $input */
    public static function dotF32(string $blocks, array $input): float
    {
        $count = intdiv(strlen($blocks), self::BLOCK_BYTES);
        $total = 0.0;
        for ($block = 0; $block < $count; $block++) {
            $offset = $block * self::BLOCK_BYTES;
            $scale = self::fp16ToF32(unpack('v', substr($blocks, $offset, 2))[1]);
            $inputOffset = $block * self::BLOCK_SIZE;
            $sum = 0.0;
            for ($index = 0; $index < 16; $index++) {
                $byte = ord($blocks[$offset + 2 + $index]);
                $sum += (($byte & 0x0F) - 8) * $input[$inputOffset + $index];
                $sum += (($byte >> 4) - 8) * $input[$inputOffset + $index + 16];
            }
            $total += $scale * $sum;
        }

        return $total;
    }

    /** @param list<float> $input @return list<float> */
    public static function matvec(string $weights, array $input, int $rows, int $columns): array
    {
        if ($columns % self::BLOCK_SIZE !== 0) {
            throw new RuntimeException('Q4_0 columns must be a multiple of 32');
        }
        $blocks = intdiv($columns, self::BLOCK_SIZE);
        $bytesPerRow = $blocks * self::BLOCK_BYTES;
        $result = [];
        for ($row = 0; $row < $rows; $row++) {
            $values = unpack('C*', substr($weights, $row * $bytesPerRow, $bytesPerRow));
            $total = 0.0;
            for ($block = 0; $block < $blocks; $block++) {
                $offset = $block * self::BLOCK_BYTES + 1;
                $scale = self::fp16ToF32($values[$offset] | ($values[$offset + 1] << 8));
                $inputIndex = $block * self::BLOCK_SIZE;
                $sum = 0.0;
                for ($index = 0; $index < 16; $index++) {
                    $byte = $values[$offset + 2 + $index];
                    $sum += (($byte & 0x0F) - 8) * $input[$inputIndex + $index];
                    $sum += (($byte >> 4) - 8) * $input[$inputIndex + $index + 16];
                }
                $total += $scale * $sum;
            }
            $result[] = $total;
        }

        return $result;
    }

    public static function dotQ8_0(string $q4Blocks, string $q8Blocks): float
    {
        $count = intdiv(strlen($q4Blocks), self::BLOCK_BYTES);
        $total = 0.0;
        for ($block = 0; $block < $count; $block++) {
            $q4Offset = $block * self::BLOCK_BYTES;
            $q8Offset = $block * 34;
            $scale = self::fp16ToF32(unpack('v', substr($q4Blocks, $q4Offset, 2))[1])
                * self::fp16ToF32(unpack('v', substr($q8Blocks, $q8Offset, 2))[1]);
            $q8 = array_values(unpack('c*', substr($q8Blocks, $q8Offset + 2, 32)));
            $sum = 0;
            for ($index = 0; $index < 16; $index++) {
                $byte = ord($q4Blocks[$q4Offset + 2 + $index]);
                $sum += (($byte & 0x0F) - 8) * $q8[$index];
                $sum += (($byte >> 4) - 8) * $q8[$index + 16];
            }
            $total += $scale * $sum;
        }

        return $total;
    }
}
