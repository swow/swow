<?php
/**
 * This file is part of Swow
 *
 * @link    https://github.com/swow/swow
 * @contact twosee <twosee@php.net>
 *
 * For the full copyright and license information,
 * please view the LICENSE file that was distributed with this source code
 */

declare(strict_types=1);

namespace Swow\Pack;

use InvalidArgumentException;

class Format
{
    public const INT8 = 'c';
    public const UINT8 = 'C';

    public const INT16 = 's';
    public const UINT16 = 'S';
    public const UINT16_BE = 'n';
    public const UINT16_LE = 'v';

    public const INT32 = 'l';
    public const UINT32 = 'L';
    public const UINT32_BE = 'N';
    public const UINT32_LE = 'V';

    public const INT64 = 'q';
    public const UINT64 = 'Q';
    public const UINT64_BE = 'J';
    public const UINT64_LE = 'P';

    public static function getSize(string $format): int
    {
        return match ($format) {
            static::INT8, static::UINT8 => 1,
            static::INT16, static::UINT16, static::UINT16_BE, static::UINT16_LE => 2,
            static::INT32, static::UINT32, static::UINT32_BE, static::UINT32_LE => 4,
            static::INT64, static::UINT64, static::UINT64_BE, static::UINT64_LE => 8,
            default => throw new InvalidArgumentException(sprintf('Unknown format type \'%s\'', $format)),
        };
    }
}
