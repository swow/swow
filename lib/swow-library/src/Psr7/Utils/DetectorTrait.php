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
use Swow\Psr7\Message\AbstractMessage;
use Swow\Psr7\Message\ServerRequest;
use Swow\Psr7\Message\UpgradeType;

use function explode;
use function method_exists;
use function strcasecmp;
use function strlen;

trait DetectorTrait
{
    public static function detectShouldKeepAlive(MessageInterface $message): bool
    {
        if (
            $message instanceof AbstractMessage ||
            method_exists($message, 'should' . 'keep' . 'alive')
        ) {
            return $message->shouldKeepAlive();
        }
        $protocolVersion = $message->getProtocolVersion();
        $parts = explode('.', $protocolVersion, 2);
        $majorVersion = (int) $parts[0];
        $minorVersion = (int) $parts[1];
        $connection = $message->getHeaderLine('connection');
        if ($majorVersion > 0 && $minorVersion > 0) {
            /* HTTP/1.1+ */
            if (strlen($connection) === strlen('close') && strcasecmp($connection, 'close') === 0) {
                return false;
            }
        } else {
            /* HTTP/1.0 or earlier */
            if (strlen($connection) !== strlen('keep-alive') || strcasecmp($connection, 'keep-alive') !== 0) {
                return false;
            }
        }
        return true;
    }

    public static function detectUpgradeType(MessageInterface $message): int
    {
        if ($message instanceof ServerRequest && !$message->isUpgrade()) {
            return UpgradeType::UPGRADE_TYPE_NONE;
        }
        return UpgradeType::fromString($message->getHeaderLine('upgrade'));
    }
}
