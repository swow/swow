<?php
/**
 * This file is part of Swow
 *
 * @link     https://github.com/swow/swow
 * @contact  twosee <twosee@php.net>
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
        switch ($format) {
            case static::INT8:
            case static::UINT8:
                return 1;
            case static::INT16:
            case static::UINT16:
            case static::UINT16_BE:
            case static::UINT16_LE:
                return 2;
            case static::INT32:
            case static::UINT32:
            case static::UINT32_BE:
            case static::UINT32_LE:
                return 4;
            case static::INT64:
            case static::UINT64:
            case static::UINT64_BE:
            case static::UINT64_LE:
                return 8;
            default:
                throw new InvalidArgumentException(sprintf('Unknown format type \'%s\'', $format));
        }
    }
}
