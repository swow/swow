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
use function explode;
use function strcasecmp;
use function strlen;
use function trim;

class UpgradeType
{
    public const UPGRADE_TYPE_NONE = 0;
    public const UPGRADE_TYPE_WEBSOCKET = 1 << 0;
    public const UPGRADE_TYPE_H2C = 1 << 1;
    public const UPGRADE_TYPE_UNKNOWN = 1 << 31;

    public static function fromString(string $upgrade): int
    {
        $upgrade = trim($upgrade);
        if ($upgrade === '') {
            return static::UPGRADE_TYPE_NONE;
        }
        $upgradeType = static::UPGRADE_TYPE_NONE;
        $upgradeList = array_map('trim', explode(',', $upgrade));
        foreach ($upgradeList as $upgradeElement) {
            $upgradeParts = explode('/', $upgradeElement, 2);
            $upgradeProtocol = $upgradeParts[0] ?? '';
            if (
                strlen($upgradeProtocol) === strlen('websocket') &&
                strcasecmp($upgradeProtocol, 'websocket') === 0
            ) {
                $upgradeType |= static::UPGRADE_TYPE_WEBSOCKET;
            } elseif (
                strlen($upgradeProtocol) === strlen('h2c') &&
                strcasecmp($upgradeProtocol, 'h2c') === 0
            ) {
                $upgradeType |= static::UPGRADE_TYPE_H2C;
            } else {
                $upgradeType |= static::UPGRADE_TYPE_UNKNOWN;
            }
        }
        return $upgradeType;
    }
}
