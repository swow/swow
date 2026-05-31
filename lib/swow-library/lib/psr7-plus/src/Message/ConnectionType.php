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

namespace Swow\Psr7\Message;

use function array_map;
use function array_pad;
use function explode;
use function strcasecmp;
use function str_contains;
use function strlen;
use function trim;

class ConnectionType
{
    public const CONNECTION_TYPE_NONE = 0;
    public const CONNECTION_TYPE_KEEP_ALIVE = 1 << 0;
    public const CONNECTION_TYPE_CLOSE = 1 << 1;

    public static function fromString(string $connection): int
    {
        $connection = trim($connection);
        if ($connection === '') {
            return static::CONNECTION_TYPE_NONE;
        }

        if (!str_contains($connection, ',')) {
            $connectionLength = strlen($connection);
            if (
                $connectionLength === strlen('keep-alive') &&
                strcasecmp($connection, 'keep-alive') === 0
            ) {
                return static::CONNECTION_TYPE_KEEP_ALIVE;
            }
            if (
                $connectionLength === strlen('close') &&
                strcasecmp($connection, 'close') === 0
            ) {
                return static::CONNECTION_TYPE_CLOSE;
            }

            return static::CONNECTION_TYPE_NONE;
        }

        $connectionType = static::CONNECTION_TYPE_NONE;
        $connectionList = array_map('trim', explode(',', $connection));
        foreach ($connectionList as $connectionElement) {
            if (
                strlen($connectionElement) === strlen('keep-alive') &&
                strcasecmp($connectionElement, 'keep-alive') === 0
            ) {
                $connectionType |= static::CONNECTION_TYPE_KEEP_ALIVE;
            } elseif (
                strlen($connectionElement) === strlen('close') &&
                strcasecmp($connectionElement, 'close') === 0
            ) {
                $connectionType |= static::CONNECTION_TYPE_CLOSE;
            }
        }

        return $connectionType;
    }

    public static function shouldKeepAlive(string $protocolVersion, string $connection): bool
    {
        $connectionType = static::fromString($connection);

        if ($connectionType & static::CONNECTION_TYPE_CLOSE) {
            return false;
        }

        if (self::isHttp11OrLater($protocolVersion)) {
            return true;
        }

        return (bool) ($connectionType & static::CONNECTION_TYPE_KEEP_ALIVE);
    }

    public static function toHeaderLine(string $protocolVersion, bool $shouldKeepAlive): ?string
    {
        if (self::isHttp2OrLater($protocolVersion)) {
            return null;
        }

        return $shouldKeepAlive ? 'keep-alive' : 'close';
    }

    private static function isHttp11OrLater(string $protocolVersion): bool
    {
        [$major, $minor] = array_pad(explode('.', $protocolVersion, 2), 2, '0');

        return (int) $major > 1 || ((int) $major === 1 && (int) $minor >= 1);
    }

    private static function isHttp2OrLater(string $protocolVersion): bool
    {
        [$major] = array_pad(explode('.', $protocolVersion, 2), 2, '0');

        return (int) $major >= 2;
    }
}
