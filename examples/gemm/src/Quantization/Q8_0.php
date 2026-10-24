<?php

declare(strict_types=1);

namespace Swow\Examples\Gemm\Quantization;

use RuntimeException;

use function abs;
use function count;
use function intdiv;
use function max;
use function min;
use function ord;
use function pack;
use function round;
use function strlen;
use function substr;
use function unpack;

final class Q8_0
{
    public const BLOCK_SIZE = 32;

    public const BLOCK_BYTES = 34;

    public static function generate(int $blocks, int $seed): string
    {
        $state = $seed;
        $data = '';
        for ($block = 0; $block < $blocks; $block++) {
            $data .= pack('v', 0x2C00);
            for ($index = 0; $index < self::BLOCK_SIZE; $index++) {
                $state ^= ($state << 13) & 0xFFFFFFFF;
                $state ^= $state >> 17;
                $state ^= ($state << 5) & 0xFFFFFFFF;
                $state &= 0xFFFFFFFF;
                $data .= pack('C', ($state >> 8) & 0xFF);
            }
        }

        return $data;
    }

    /** @param list<float> $input */
    public static function quantize(array $input): string
    {
        if (count($input) % self::BLOCK_SIZE !== 0) {
            throw new RuntimeException('Q8_0 input length must be a multiple of 32');
        }

        $data = '';
        $blocks = intdiv(count($input), self::BLOCK_SIZE);
        for ($block = 0; $block < $blocks; $block++) {
            $base = $block * self::BLOCK_SIZE;
            $amax = 0.0;
            for ($index = 0; $index < self::BLOCK_SIZE; $index++) {
                $value = abs($input[$base + $index]);
                if ($value > $amax) {
                    $amax = $value;
                }
            }

            // Use the raw float scale for q and store the fp16-rounded scale
            $scale = unpack('G', pack('G', $amax / 127.0))[1];
            $data .= self::block($input, $base, $scale);
        }

        return $data;
    }

    /** @param list<float> $input */
    public static function quantizeFixed(array $input, float $scale = 0.0625): string
    {
        if (count($input) % self::BLOCK_SIZE !== 0) {
            throw new RuntimeException('Q8_0 input length must be a multiple of 32');
        }

        $half = self::fp32ToF16($scale);
        $scale = Q4_0::fp16ToF32($half);
        $data = '';
        for ($block = 0; $block < intdiv(count($input), self::BLOCK_SIZE); $block++) {
            $data .= self::block($input, $block * self::BLOCK_SIZE, $scale);
        }

        return $data;
    }

    /** @param list<float> $input */
    private static function block(array $input, int $base, float $scale): string
    {
        $inverse = $scale !== 0.0 ? 1.0 / $scale : 0.0;
        $data = pack('v', self::fp32ToF16($scale));
        for ($index = 0; $index < self::BLOCK_SIZE; $index++) {
            $value = (int) round($input[$base + $index] * $inverse);
            $data .= pack('c', max(-128, min(127, $value)));
        }

        return $data;
    }

    /** @return list<float> */
    public static function dequantize(string $blocks): array
    {
        $count = intdiv(strlen($blocks), self::BLOCK_BYTES);
        $values = [];
        for ($block = 0; $block < $count; $block++) {
            $offset = $block * self::BLOCK_BYTES;
            $scale = Q4_0::fp16ToF32(unpack('v', substr($blocks, $offset, 2))[1]);
            for ($index = 0; $index < self::BLOCK_SIZE; $index++) {
                $value = ord($blocks[$offset + 2 + $index]);
                if ($value >= 128) {
                    $value -= 256;
                }
                $values[] = $value * $scale;
            }
        }

        return $values;
    }

    public static function dotQ8_0(string $left, string $right): float
    {
        $count = intdiv(strlen($left), self::BLOCK_BYTES);
        $total = 0.0;
        for ($block = 0; $block < $count; $block++) {
            $leftOffset = $block * self::BLOCK_BYTES;
            $rightOffset = $block * self::BLOCK_BYTES;
            $scale = Q4_0::fp16ToF32(unpack('v', substr($left, $leftOffset, 2))[1])
                * Q4_0::fp16ToF32(unpack('v', substr($right, $rightOffset, 2))[1]);
            $sum = 0;
            for ($index = 0; $index < self::BLOCK_SIZE; $index++) {
                $a = ord($left[$leftOffset + 2 + $index]);
                $b = ord($right[$rightOffset + 2 + $index]);
                $sum += (($a ^ 0x80) - 0x80) * (($b ^ 0x80) - 0x80);
            }
            $total += $sum * $scale;
        }

        return $total;
    }

    /** @param list<float> $input @return list<float> */
    public static function matvec(string $weights, array $input, int $rows, int $columns): array
    {
        if ($columns % self::BLOCK_SIZE !== 0) {
            throw new RuntimeException('Q8_0 columns must be a multiple of 32');
        }
        $blocks = intdiv($columns, self::BLOCK_SIZE);
        $bytesPerRow = $blocks * self::BLOCK_BYTES;
        $quantized = unpack('c*', self::quantize($input));
        $inputValues = [];
        $inputScales = [];
        for ($block = 0; $block < $blocks; $block++) {
            $offset = $block * self::BLOCK_BYTES + 1;
            $half = ($quantized[$offset] & 0xFF) | (($quantized[$offset + 1] & 0xFF) << 8);
            $inputScales[$block] = Q4_0::fp16ToF32($half);
            for ($index = 0; $index < self::BLOCK_SIZE; $index++) {
                $inputValues[] = $quantized[$offset + 2 + $index];
            }
        }

        $result = [];
        for ($row = 0; $row < $rows; $row++) {
            $values = unpack('c*', substr($weights, $row * $bytesPerRow, $bytesPerRow));
            $total = 0.0;
            for ($block = 0; $block < $blocks; $block++) {
                $offset = $block * self::BLOCK_BYTES + 1;
                $half = ($values[$offset] & 0xFF) | (($values[$offset + 1] & 0xFF) << 8);
                $scale = Q4_0::fp16ToF32($half) * $inputScales[$block];
                $sum = 0;
                $index = $offset + 2;
                $inputIndex = $block * self::BLOCK_SIZE;
                for ($step = 0; $step < self::BLOCK_SIZE; $step += 4) {
                    $sum += $values[$index] * $inputValues[$inputIndex]
                        + $values[$index + 1] * $inputValues[$inputIndex + 1]
                        + $values[$index + 2] * $inputValues[$inputIndex + 2]
                        + $values[$index + 3] * $inputValues[$inputIndex + 3];
                    $index += 4;
                    $inputIndex += 4;
                }
                $total += $sum * $scale;
            }
            $result[] = $total;
        }

        return $result;
    }

    /**
     * Quantized matrix times f32 matrix, one column at a time.
     *
     * @param list<float> $right
     * @return list<float>
     */
    public static function gemm(string $left, int $rows, int $columns, array $right, int $rightColumns): array
    {
        if ($columns % self::BLOCK_SIZE !== 0) {
            throw new RuntimeException('Q8_0 columns must be a multiple of 32');
        }
        $blocksPerRow = intdiv($columns, self::BLOCK_SIZE);
        $bytesPerRow = $blocksPerRow * self::BLOCK_BYTES;
        $result = [];
        for ($column = 0; $column < $rightColumns; $column++) {
            $values = [];
            for ($row = 0; $row < $columns; $row++) {
                $values[] = $right[$row * $rightColumns + $column];
            }
            $quantized = self::quantize($values);
            for ($row = 0; $row < $rows; $row++) {
                $result[$row * $rightColumns + $column] = self::dotQ8_0(
                    substr($left, $row * $bytesPerRow, $bytesPerRow),
                    $quantized,
                );
            }
        }

        return $result;
    }

    public static function fp32ToF16(float $value): int
    {
        $bits = unpack('N', pack('G', $value))[1];
        $sign = ($bits >> 16) & 0x8000;
        $exponent = ($bits >> 23) & 0xFF;
        $mantissa = $bits & 0x7FFFFF;

        if ($exponent === 0xFF) {
            return $sign | 0x7C00 | ($mantissa !== 0 ? 0x200 : 0);
        }
        $exponent -= 127;
        if ($exponent > 15) {
            return $sign | 0x7C00;
        }
        if ($exponent < -24) {
            return $sign;
        }
        if ($exponent < -14) {
            $mantissa |= 0x800000;
            $shift = -$exponent - 14 + 13;
            $shifted = $mantissa >> $shift;
            $remainder = $mantissa & ((1 << $shift) - 1);
            $half = 1 << ($shift - 1);
            if ($remainder > $half || ($remainder === $half && ($shifted & 1))) {
                $shifted++;
            }

            return $sign | $shifted;
        }

        $halfExponent = $exponent + 15;
        $shifted = $mantissa >> 13;
        $remainder = $mantissa & 0x1FFF;
        if ($remainder > 0x1000 || ($remainder === 0x1000 && ($shifted & 1))) {
            $shifted++;
            if ($shifted === 0x400) {
                $shifted = 0;
                $halfExponent++;
                if ($halfExponent >= 31) {
                    return $sign | 0x7C00;
                }
            }
        }

        return $sign | ($halfExponent << 10) | $shifted;
    }
}
