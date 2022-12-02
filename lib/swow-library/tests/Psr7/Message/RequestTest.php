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
use Swow\Psr7\Psr7;

/**
 * @internal
 * @covers \Swow\Psr7\Message\Request
 * @covers \Swow\Psr7\Psr7
 */
final class RequestTest extends TestCase
{
    public function testGetRequestTarget(): void
    {
        $request = Psr7::createRequest('GET', '/request');
        $this->assertSame('/request', $request->getRequestTarget());

        $request = Psr7::createRequest('POST', Psr7::createUriFromString('/test'));
        $this->assertSame('POST', $request->getMethod());
        $this->assertSame('/test', $request->getRequestTarget());
    }
}
