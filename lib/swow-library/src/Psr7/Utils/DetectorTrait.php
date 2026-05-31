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

namespace Swow\Psr7\Utils;

use Psr\Http\Message\MessageInterface;
use Swow\Psr7\Message\ConnectionType;
use Swow\Psr7\Message\MessagePlusInterface;
use Swow\Psr7\Message\ServerRequest;
use Swow\Psr7\Message\UpgradeType;

trait DetectorTrait
{
    public static function detectShouldKeepAlive(MessageInterface $message): bool
    {
        if ($message instanceof MessagePlusInterface) {
            return $message->shouldKeepAlive();
        }

        return ConnectionType::shouldKeepAlive(
            $message->getProtocolVersion(),
            $message->getHeaderLine('Connection')
        );
    }

    public static function detectUpgradeType(MessageInterface $message): int
    {
        if ($message instanceof ServerRequest && $message->isUpgrade() === false) {
            return UpgradeType::UPGRADE_TYPE_NONE;
        }
        return UpgradeType::fromString($message->getHeaderLine('upgrade'));
    }
}
