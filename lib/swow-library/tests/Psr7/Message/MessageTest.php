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

use PHPUnit\Framework\TestCase;
use Swow\Psr7\Message\MessagePlusInterface;
use Swow\Psr7\Psr7;

/**
 * @internal
 * @covers \Swow\Psr7\Message\AbstractMessage
 * @covers \Swow\Psr7\Psr7
 */
final class MessageTest extends TestCase
{
    public function testAddHeader(): void
    {
        foreach ([false, true] as $useWith) {
            $request = Psr7::createRequest('GET', '/request', ['foo' => 'bar']);
            if (!$useWith && !($request instanceof MessagePlusInterface)) {
                continue;
            }
            $this->assertSame($request->getHeaders(), ['foo' => ['bar']]);
            if (!$useWith) {
                $request->addHeader('foo', 'baz');
            } else {
                $request = $request->withAddedHeader('foo', 'baz');
            }
            $this->assertSame($request->getHeader('foo'), ['bar', 'baz']);
            if (!$useWith) {
                $request->addHeader('char', 'dua');
            } else {
                $request = $request->withAddedHeader('char', 'dua');
            }
            $this->assertSame($request->getHeader('char'), ['dua']);
            $this->assertSame($request->getHeaderLine('char'), 'dua');
            $this->assertSame($request->getHeaders(), ['foo' => ['bar', 'baz'], 'char' => ['dua']]);
        }
    }
}
