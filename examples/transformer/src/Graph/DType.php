<?php

declare(strict_types=1);

namespace Swow\Examples\Transformer\Graph;

final class DType
{
    public const F32 = 'f32';

    public const I32 = 'i32';

    public static function assertSupported(string $dtype): void
    {
        if ($dtype !== self::F32 && $dtype !== self::I32) {
            throw new GraphException("Unsupported dtype: {$dtype}");
        }
    }
}
