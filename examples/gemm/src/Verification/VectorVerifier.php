<?php

declare(strict_types=1);

namespace Swow\Examples\Gemm\Verification;

use RuntimeException;
use Swow\Examples\Gemm\Quantization\Q4_0;
use Swow\Examples\Gemm\Quantization\Q8_0;

use function abs;
use function array_fill;
use function array_map;
use function array_slice;
use function count;
use function file;
use function hex2bin;
use function hexdec;
use function intdiv;
use function max;
use function ord;
use function preg_match;
use function preg_split;
use function printf;
use function str_starts_with;
use function strlen;
use function trim;

use const FILE_IGNORE_NEW_LINES;

final class VectorVerifier
{
    private const RELATIVE_TOLERANCE = 1e-4;

    private const ABSOLUTE_TOLERANCE = 1e-5;

    public static function verify(string $path): bool
    {
        $sections = self::parse($path);
        $passed = true;

        $checks = [
            'fp16' => static fn(array $fields): array => self::runFp16($fields),
            'q4_0_dequant' => static fn(array $fields): array => self::runQ4Dequant($fields),
            'q4_0_dot_f32' => static fn(array $fields): array => self::runQ4DotF32($fields),
            'q4_0_dot_q8_0' => static fn(array $fields): array => self::runQ4DotQ8_0($fields),
            'matvec_q4_0' => static fn(array $fields): array => self::runMatvecQ4_0($fields),
            'q8_0_dequant' => static fn(array $fields): array => self::runQ8Dequant($fields),
            'q8_0_dot_q8_0' => static fn(array $fields): array => self::runQ8DotQ8_0($fields),
            'matvec_q8_0' => static fn(array $fields): array => self::runMatvecQ8_0($fields),
            'gemm_q8_0' => static fn(array $fields): array => self::runGemmQ8_0($fields),
            'gemm_f32' => static fn(array $fields): array => self::runGemm($fields),
            'matvec_f32' => static fn(array $fields): array => self::runMatvec($fields),
        ];

        foreach ($checks as $name => $runner) {
            if (!isset($sections[$name])) {
                printf("%s: missing\n", $name);
                $passed = false;
                continue;
            }

            $worstAbsolute = 0.0;
            $worstRelative = 0.0;
            $failures = 0;
            foreach ($sections[$name] as $fields) {
                [$actual, $expected] = $runner($fields);
                foreach ($expected as $index => $value) {
                    $difference = abs($actual[$index] - $value);
                    $relative = $value !== 0.0 ? $difference / abs($value) : 0.0;
                    $worstAbsolute = max($worstAbsolute, $difference);
                    $worstRelative = max($worstRelative, $relative);
                    $allowed = max(self::RELATIVE_TOLERANCE * abs($value), self::ABSOLUTE_TOLERANCE);
                    if ($difference > $allowed) {
                        $failures++;
                    }
                }
            }

            printf(
                "%s: count=%d worst_absolute=%.3e worst_relative=%.3e %s\n",
                $name,
                count($sections[$name]),
                $worstAbsolute,
                $worstRelative,
                $failures === 0 ? 'PASS' : 'FAIL',
            );
            if ($failures !== 0) {
                printf("%s: %d value(s) outside tolerance\n", $name, $failures);
                $passed = false;
            }
        }

        if (!isset($sections['q8_0_quant'])) {
            printf("q8_0_quant: missing\n");
            $passed = false;
        } else {
            $worstByte = 0;
            $failures = 0;
            foreach ($sections['q8_0_quant'] as $fields) {
                $actual = Q8_0::quantizeFixed(self::toFloats(array_slice($fields, 0, Q8_0::BLOCK_SIZE)));
                $expected = self::binary($fields[Q8_0::BLOCK_SIZE]);
                if (strlen($actual) !== strlen($expected)) {
                    $failures++;
                    continue;
                }
                for ($index = 0; $index < strlen($actual); $index++) {
                    $difference = abs(ord($actual[$index]) - ord($expected[$index]));
                    $worstByte = max($worstByte, $difference);
                    if ($difference > 1) {
                        $failures++;
                    }
                }
            }
            printf(
                "q8_0_quant: count=%d worst_byte_difference=%d %s\n",
                count($sections['q8_0_quant']),
                $worstByte,
                $failures === 0 ? 'PASS' : 'FAIL',
            );
            if ($failures !== 0) {
                $passed = false;
            }
        }

        return $passed;
    }

    /** @return array<string, list<list<string>>> */
    private static function parse(string $path): array
    {
        $lines = file($path, FILE_IGNORE_NEW_LINES);
        if ($lines === false) {
            throw new RuntimeException("Unable to read {$path}");
        }

        $sections = [];
        $current = null;
        foreach ($lines as $line) {
            $line = trim($line);
            if ($line === '' || str_starts_with($line, '#')) {
                continue;
            }
            if (preg_match('/^\[(.+)]$/', $line, $matches) === 1) {
                $current = $matches[1];
                $sections[$current] = [];
                continue;
            }
            if ($current === null || str_starts_with($line, 'count ')) {
                continue;
            }
            $fields = preg_split('/\s+/', $line);
            if ($fields === false) {
                throw new RuntimeException("Unable to parse {$path}");
            }
            $sections[$current][] = $fields;
        }

        return $sections;
    }

    /** @param list<string> $fields @return array{0: list<float>, 1: list<float>} */
    private static function runFp16(array $fields): array
    {
        return [
            [Q4_0::fp16ToF32((int) hexdec($fields[0]))],
            [(float) $fields[1]],
        ];
    }

    /** @param list<string> $fields @return array{0: list<float>, 1: list<float>} */
    private static function runQ4Dequant(array $fields): array
    {
        return [
            Q4_0::dequantize(self::binary($fields[0])),
            self::toFloats(array_slice($fields, 1, 32)),
        ];
    }

    /** @param list<string> $fields @return array{0: list<float>, 1: list<float>} */
    private static function runQ4DotF32(array $fields): array
    {
        return [
            [Q4_0::dotF32(self::binary($fields[0]), self::toFloats(array_slice($fields, 1, 32)))],
            [(float) $fields[33]],
        ];
    }

    /** @param list<string> $fields @return array{0: list<float>, 1: list<float>} */
    private static function runQ4DotQ8_0(array $fields): array
    {
        return [
            [Q4_0::dotQ8_0(self::binary($fields[0]), self::binary($fields[1]))],
            [(float) $fields[2]],
        ];
    }

    /** @param list<string> $fields @return array{0: list<float>, 1: list<float>} */
    private static function runMatvecQ4_0(array $fields): array
    {
        $rows = (int) $fields[0];
        $columns = (int) $fields[1];
        $blocksPerRow = intdiv($columns, Q4_0::BLOCK_SIZE);
        $weights = '';
        for ($row = 0; $row < $rows; $row++) {
            for ($block = 0; $block < $blocksPerRow; $block++) {
                $weights .= self::binary($fields[2 + $row * $blocksPerRow + $block]);
            }
        }
        $inputOffset = 2 + $rows * $blocksPerRow;
        $input = self::toFloats(array_slice($fields, $inputOffset, $columns));
        $expected = self::toFloats(array_slice($fields, $inputOffset + $columns, $rows));

        return [Q4_0::matvec($weights, $input, $rows, $columns), $expected];
    }

    /** @param list<string> $fields @return array{0: list<float>, 1: list<float>} */
    private static function runQ8Dequant(array $fields): array
    {
        return [
            Q8_0::dequantize(self::binary($fields[0])),
            self::toFloats(array_slice($fields, 1, Q8_0::BLOCK_SIZE)),
        ];
    }

    /** @param list<string> $fields @return array{0: list<float>, 1: list<float>} */
    private static function runQ8DotQ8_0(array $fields): array
    {
        return [
            [Q8_0::dotQ8_0(self::binary($fields[0]), self::binary($fields[1]))],
            [(float) $fields[2]],
        ];
    }

    /** @param list<string> $fields @return array{0: list<float>, 1: list<float>} */
    private static function runMatvecQ8_0(array $fields): array
    {
        $rows = (int) $fields[0];
        $columns = (int) $fields[1];
        $blocksPerRow = intdiv($columns, Q8_0::BLOCK_SIZE);
        $weights = '';
        for ($row = 0; $row < $rows; $row++) {
            for ($block = 0; $block < $blocksPerRow; $block++) {
                $weights .= self::binary($fields[2 + $row * $blocksPerRow + $block]);
            }
        }
        $inputOffset = 2 + $rows * $blocksPerRow;
        $input = self::toFloats(array_slice($fields, $inputOffset, $columns));
        $expected = self::toFloats(array_slice($fields, $inputOffset + $columns, $rows));

        return [Q8_0::matvec($weights, $input, $rows, $columns), $expected];
    }

    /** @param list<string> $fields @return array{0: list<float>, 1: list<float>} */
    private static function runGemmQ8_0(array $fields): array
    {
        $rows = (int) $fields[0];
        $rightColumns = (int) $fields[1];
        $columns = (int) $fields[2];
        $blocksPerRow = intdiv($columns, Q8_0::BLOCK_SIZE);
        $left = '';
        for ($row = 0; $row < $rows; $row++) {
            for ($block = 0; $block < $blocksPerRow; $block++) {
                $left .= self::binary($fields[3 + $row * $blocksPerRow + $block]);
            }
        }
        $offset = 3 + $rows * $blocksPerRow;
        $right = self::toFloats(array_slice($fields, $offset, $columns * $rightColumns));
        $expected = self::toFloats(array_slice($fields, $offset + $columns * $rightColumns, $rows * $rightColumns));

        return [Q8_0::gemm($left, $rows, $columns, $right, $rightColumns), $expected];
    }

    /** @param list<string> $fields @return array{0: list<float>, 1: list<float>} */
    private static function runGemm(array $fields): array
    {
        $m = (int) $fields[0];
        $n = (int) $fields[1];
        $k = (int) $fields[2];
        $offset = 3;
        $a = self::toFloats(array_slice($fields, $offset, $m * $k));
        $offset += $m * $k;
        $b = self::toFloats(array_slice($fields, $offset, $k * $n));
        $offset += $k * $n;
        $expected = self::toFloats(array_slice($fields, $offset, $m * $n));

        return [self::multiply($a, $b, $m, $n, $k), $expected];
    }

    /** @param list<string> $fields @return array{0: list<float>, 1: list<float>} */
    private static function runMatvec(array $fields): array
    {
        $rows = (int) $fields[0];
        $columns = (int) $fields[1];
        $a = self::toFloats(array_slice($fields, 2, $rows * $columns));
        $input = self::toFloats(array_slice($fields, 2 + $rows * $columns, $columns));
        $expected = self::toFloats(array_slice($fields, 2 + $rows * $columns + $columns, $rows));

        return [self::multiply($a, $input, $rows, 1, $columns), $expected];
    }

    /** @param list<float> $a @param list<float> $b @return list<float> */
    private static function multiply(array $a, array $b, int $m, int $n, int $k): array
    {
        $c = array_fill(0, $m * $n, 0.0);
        for ($row = 0; $row < $m; $row++) {
            for ($column = 0; $column < $n; $column++) {
                $sum = 0.0;
                for ($index = 0; $index < $k; $index++) {
                    $sum += $a[$row * $k + $index] * $b[$index * $n + $column];
                }
                $c[$row * $n + $column] = $sum;
            }
        }

        return $c;
    }

    /** @param list<string> $fields @return list<float> */
    private static function toFloats(array $fields): array
    {
        return array_map(static fn(string $value): float => (float) $value, $fields);
    }

    private static function binary(string $hex): string
    {
        $data = hex2bin($hex);
        if ($data === false) {
            throw new RuntimeException('Invalid hexadecimal test vector');
        }

        return $data;
    }
}
