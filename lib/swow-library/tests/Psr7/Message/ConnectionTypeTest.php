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
use Swow\Psr7\Message\ConnectionType;

/**
 * @internal
 */
#[CoversClass(ConnectionType::class)]
final class ConnectionTypeTest extends TestCase
{
    #[DataProvider('provideFromStringParsesConnectionTokensCases')]
    public function testFromStringParsesConnectionTokens(string $connection, int $expected): void
    {
        $this->assertSame($expected, ConnectionType::fromString($connection));
    }

    public static function provideFromStringParsesConnectionTokensCases(): iterable
    {
        yield 'empty' => [
            '',
            ConnectionType::CONNECTION_TYPE_NONE,
        ];
        yield 'whitespace only' => [
            " \t\n\v\r\0 ",
            ConnectionType::CONNECTION_TYPE_NONE,
        ];
        yield 'close' => [
            'close',
            ConnectionType::CONNECTION_TYPE_CLOSE,
        ];
        yield 'close with case and whitespace' => [
            "\tClose\t",
            ConnectionType::CONNECTION_TYPE_CLOSE,
        ];
        yield 'close with all trim bytes' => [
            "\0\t\n\v\r close \0\t\n\v\r",
            ConnectionType::CONNECTION_TYPE_CLOSE,
        ];
        yield 'keep alive' => [
            'keep-alive',
            ConnectionType::CONNECTION_TYPE_KEEP_ALIVE,
        ];
        yield 'keep alive with case and whitespace' => [
            ' Keep-Alive ',
            ConnectionType::CONNECTION_TYPE_KEEP_ALIVE,
        ];
        yield 'empty token list' => [
            ',,,',
            ConnectionType::CONNECTION_TYPE_NONE,
        ];
        yield 'close after empty token' => [
            ', close',
            ConnectionType::CONNECTION_TYPE_CLOSE,
        ];
        yield 'close before empty token' => [
            'close,',
            ConnectionType::CONNECTION_TYPE_CLOSE,
        ];
        yield 'close token in list' => [
            'foo, close',
            ConnectionType::CONNECTION_TYPE_CLOSE,
        ];
        yield 'close token without list whitespace' => [
            'foo,close',
            ConnectionType::CONNECTION_TYPE_CLOSE,
        ];
        yield 'keep alive token in list' => [
            'upgrade, keep-alive, foo',
            ConnectionType::CONNECTION_TYPE_KEEP_ALIVE,
        ];
        yield 'keep alive token in case-insensitive list' => [
            'Upgrade, KEEP-ALIVE, Foo',
            ConnectionType::CONNECTION_TYPE_KEEP_ALIVE,
        ];
        yield 'keep alive and close in list' => [
            'keep-alive, close',
            ConnectionType::CONNECTION_TYPE_KEEP_ALIVE | ConnectionType::CONNECTION_TYPE_CLOSE,
        ];
        yield 'close and keep alive in list' => [
            'close, keep-alive',
            ConnectionType::CONNECTION_TYPE_KEEP_ALIVE | ConnectionType::CONNECTION_TYPE_CLOSE,
        ];
        yield 'duplicate tokens collapse to bit flags' => [
            'close, close, keep-alive, keep-alive',
            ConnectionType::CONNECTION_TYPE_KEEP_ALIVE | ConnectionType::CONNECTION_TYPE_CLOSE,
        ];
        yield 'close prefix is not a token' => [
            'xclose',
            ConnectionType::CONNECTION_TYPE_NONE,
        ];
        yield 'close suffix is not a token' => [
            'closex',
            ConnectionType::CONNECTION_TYPE_NONE,
        ];
        yield 'close with parameter is not a token' => [
            'close; foo=bar',
            ConnectionType::CONNECTION_TYPE_NONE,
        ];
        yield 'close in parameter is not a token' => [
            'foo=close',
            ConnectionType::CONNECTION_TYPE_NONE,
        ];
        yield 'keep alive with suffix is not a token' => [
            'keep-alive-close',
            ConnectionType::CONNECTION_TYPE_NONE,
        ];
        yield 'keep alive with parameter is not a token' => [
            'keep-alive; foo=bar',
            ConnectionType::CONNECTION_TYPE_NONE,
        ];
        yield 'keep alive in parameter is not a token' => [
            'foo=keep-alive',
            ConnectionType::CONNECTION_TYPE_NONE,
        ];
        yield 'space separated values are not tokens' => [
            'keep-alive close',
            ConnectionType::CONNECTION_TYPE_NONE,
        ];
        yield 'invalid list tokens do not affect valid token' => [
            'xclose, keep-alivex, keep-alive',
            ConnectionType::CONNECTION_TYPE_KEEP_ALIVE,
        ];
    }

    #[DataProvider('provideShouldKeepAliveFollowsProtocolAndConnectionHeaderCases')]
    public function testShouldKeepAliveFollowsProtocolAndConnectionHeader(
        string $protocolVersion,
        string $connection,
        bool $expected,
    ): void {
        $this->assertSame($expected, ConnectionType::shouldKeepAlive($protocolVersion, $connection));
    }

    public static function provideShouldKeepAliveFollowsProtocolAndConnectionHeaderCases(): iterable
    {
        yield 'http 1.1 keeps alive by default' => [
            '1.1',
            '',
            true,
        ];
        yield 'http 1.0 closes by default' => [
            '1.0',
            '',
            false,
        ];
        yield 'http 1.0 keeps alive with explicit token' => [
            '1.0',
            'keep-alive',
            true,
        ];
        yield 'http 1.0 closes with unrelated token' => [
            '1.0',
            'upgrade',
            false,
        ];
        yield 'http 1.1 close disables keep alive' => [
            '1.1',
            'close',
            false,
        ];
        yield 'http 1.0 close disables keep alive' => [
            '1.0',
            'close',
            false,
        ];
        yield 'http 1.1 ignores invalid close substring' => [
            '1.1',
            'xclose',
            true,
        ];
        yield 'http 1.0 ignores invalid keep alive substring' => [
            '1.0',
            'keep-alivex',
            false,
        ];
        yield 'http 1.0 keeps alive when valid token appears in list' => [
            '1.0',
            'upgrade, keep-alive',
            true,
        ];
        yield 'http 1.1 close wins in list' => [
            '1.1',
            'upgrade, keep-alive, close',
            false,
        ];
        yield 'http 1.0 close wins in list' => [
            '1.0',
            'upgrade, keep-alive, close',
            false,
        ];
        yield 'http 1.1 minor greater than one keeps alive by default' => [
            '1.10',
            '',
            true,
        ];
        yield 'http 1 without minor closes by default' => [
            '1',
            '',
            false,
        ];
        yield 'http 2 keeps alive by default' => [
            '2',
            '',
            true,
        ];
        yield 'http 2 close still disables keep alive' => [
            '2.0',
            'close',
            false,
        ];
        yield 'http 3 keeps alive by default' => [
            '3.0',
            '',
            true,
        ];
    }

    #[DataProvider('provideToHeaderLineFollowsProtocolVersionCases')]
    public function testToHeaderLineFollowsProtocolVersion(
        string $protocolVersion,
        bool $shouldKeepAlive,
        ?string $expected,
    ): void {
        $this->assertSame($expected, ConnectionType::toHeaderLine($protocolVersion, $shouldKeepAlive));
    }

    public static function provideToHeaderLineFollowsProtocolVersionCases(): iterable
    {
        yield 'http 1.0 keep alive' => [
            '1.0',
            true,
            'keep-alive',
        ];
        yield 'http 1.0 close' => [
            '1.0',
            false,
            'close',
        ];
        yield 'http 1.1 keep alive' => [
            '1.1',
            true,
            'keep-alive',
        ];
        yield 'http 1.1 close' => [
            '1.1',
            false,
            'close',
        ];
        yield 'http 1 without minor keep alive' => [
            '1',
            true,
            'keep-alive',
        ];
        yield 'http 2 omits keep alive header' => [
            '2',
            true,
            null,
        ];
        yield 'http 2 omits close header' => [
            '2.0',
            false,
            null,
        ];
        yield 'http 3 omits keep alive header' => [
            '3.0',
            true,
            null,
        ];
    }

    public function testCloseWinsOverKeepAlive(): void
    {
        $this->assertFalse(ConnectionType::shouldKeepAlive('1.1', 'keep-alive, close'));
        $this->assertFalse(ConnectionType::shouldKeepAlive('1.0', 'keep-alive, close'));
        $this->assertFalse(ConnectionType::shouldKeepAlive('2.0', 'keep-alive, close'));
    }
}
