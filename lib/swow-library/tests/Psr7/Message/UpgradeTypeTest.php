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

namespace Swow\Tests\Psr7\Message;

use PHPUnit\Framework\Attributes\CoversClass;
use PHPUnit\Framework\Attributes\DataProvider;
use PHPUnit\Framework\TestCase;
use Swow\Psr7\Message\UpgradeType;

/** @internal */
#[CoversClass(UpgradeType::class)]
final class UpgradeTypeTest extends TestCase
{
    #[DataProvider('provideProtocolListsPreserveKnownAndUnknownTypesCases')]
    public function testProtocolListsPreserveKnownAndUnknownTypes(string $header, int $types): void
    {
        $this->assertSame($types, UpgradeType::fromString($header));
    }

    /** @return iterable<string, array{string, int}> */
    public static function provideProtocolListsPreserveKnownAndUnknownTypesCases(): iterable
    {
        yield 'empty' => ['', UpgradeType::UPGRADE_TYPE_NONE];
        yield 'whitespace' => [" \t ", UpgradeType::UPGRADE_TYPE_NONE];
        yield 'websocket' => ['websocket', UpgradeType::UPGRADE_TYPE_WEBSOCKET];
        yield 'websocket case and whitespace' => [' WebSocket ', UpgradeType::UPGRADE_TYPE_WEBSOCKET];
        yield 'versioned websocket' => ['websocket/13', UpgradeType::UPGRADE_TYPE_WEBSOCKET];
        yield 'h2c' => ['h2c', UpgradeType::UPGRADE_TYPE_H2C];
        yield 'versioned h2c' => ['H2C/1.0', UpgradeType::UPGRADE_TYPE_H2C];
        yield 'both known protocols' => ['websocket, h2c', UpgradeType::UPGRADE_TYPE_WEBSOCKET | UpgradeType::UPGRADE_TYPE_H2C];
        yield 'duplicates' => ['websocket, WebSocket', UpgradeType::UPGRADE_TYPE_WEBSOCKET];
        yield 'unknown protocol' => ['custom/2', UpgradeType::UPGRADE_TYPE_UNKNOWN];
        yield 'matching suffix is insufficient' => ['xwebsocket', UpgradeType::UPGRADE_TYPE_UNKNOWN];
        yield 'matching prefix is insufficient' => ['h2c-extra', UpgradeType::UPGRADE_TYPE_UNKNOWN];
        yield 'known and unknown protocols' => ['custom/2, websocket, H2C/1.0', UpgradeType::UPGRADE_TYPE_UNKNOWN | UpgradeType::UPGRADE_TYPE_WEBSOCKET | UpgradeType::UPGRADE_TYPE_H2C];
    }
}
