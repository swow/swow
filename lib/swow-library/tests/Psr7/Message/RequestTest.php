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
use Swow\Psr7\Message\Request;
use Swow\Psr7\Message\ServerRequest;
use Swow\Psr7\Message\Uri;

/**
 * @internal
 * @coversNothing
 */
final class RequestTest extends TestCase
{
    public function testGetRequestTarget(): void
    {
        $request = new Request();
        $this->assertSame('/', $request->getRequestTarget());

        $request = new ServerRequest();
        $this->assertSame('/', $request->getRequestTarget());

        $request = new Request('GET', '/index');
        $this->assertSame('/index', $request->getRequestTarget());

        $request = new Request('GET', new Uri('/test'));
        $this->assertSame('/test', $request->getRequestTarget());
    }
}
