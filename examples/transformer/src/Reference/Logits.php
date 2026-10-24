<?php

declare(strict_types=1);

namespace Swow\Examples\Transformer\Reference;

use RuntimeException;

use function abs;
use function array_slice;
use function count;
use function file_get_contents;
use function is_file;
use function max;
use function strlen;
use function unpack;
use function usort;

final class Logits
{
    /** @return list<float> */
    public static function read(string $path): array
    {
        if (!is_file($path)) {
            throw new RuntimeException("Logits file does not exist: {$path}");
        }
        $bytes = file_get_contents($path);
        if ($bytes === false || strlen($bytes) % 4 !== 0) {
            throw new RuntimeException("Invalid logits file: {$path}");
        }

        return array_values(unpack('g*', $bytes));
    }

    /**
     * @param list<float> $reference
     * @param list<float> $actual
     * @return array{
     *     count: int,
     *     max_abs: float,
     *     max_rel: float,
     *     top_reference: list<int>,
     *     top_actual: list<int>,
     *     top1_match: bool,
     *     top5_match: bool
     * }
     */
    public static function compare(array $reference, array $actual, int $top = 5): array
    {
        if (count($reference) !== count($actual)) {
            throw new RuntimeException('Logits length mismatch');
        }
        $maxAbs = 0.0;
        $maxRel = 0.0;
        foreach ($reference as $index => $value) {
            $difference = abs($actual[$index] - $value);
            $maxAbs = max($maxAbs, $difference);
            if (abs($value) > 1e-6) {
                $maxRel = max($maxRel, $difference / abs($value));
            }
        }

        $referenceTop = self::top($reference, $top);
        $actualTop = self::top($actual, $top);

        return [
            'count' => count($reference),
            'max_abs' => $maxAbs,
            'max_rel' => $maxRel,
            'top_reference' => $referenceTop,
            'top_actual' => $actualTop,
            'top1_match' => $referenceTop[0] === $actualTop[0],
            'top5_match' => $referenceTop === $actualTop,
        ];
    }

    /** @param list<float> $values @return list<int> */
    private static function top(array $values, int $count): array
    {
        $indexes = range(0, count($values) - 1);
        usort(
            $indexes,
            static fn(int $left, int $right): int => $values[$right] <=> $values[$left],
        );

        return array_slice($indexes, 0, $count);
    }
}
