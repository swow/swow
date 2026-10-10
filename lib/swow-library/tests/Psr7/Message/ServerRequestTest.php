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
use PHPUnit\Framework\TestCase;
use Swow\Psr7\Message\BodyDecoder;
use Swow\Psr7\Message\ServerRequest;
use Swow\Psr7\Message\UploadedFile;
use Swow\Psr7\Psr7;

use function strlen;

/** @internal */
#[CoversClass(ServerRequest::class)]
#[CoversClass(BodyDecoder::class)]
final class ServerRequestTest extends TestCase
{
    public function testRequestMetadataCopiesAreIndependent(): void
    {
        $request = new ServerRequest();
        $copy = $request->withServerParams(['REMOTE_ADDR' => '127.0.0.1'])
            ->withCookieParams(['session' => 'cookie'])->withQueryParams(['page' => '2']);
        $this->assertSame([], $request->getServerParams());
        $this->assertSame([], $request->getCookieParams());
        $this->assertSame([], $request->getQueryParams());
        $this->assertSame(['REMOTE_ADDR' => '127.0.0.1'], $copy->getServerParams());
        $this->assertSame(['session' => 'cookie'], $copy->getCookieParams());
        $this->assertSame(['page' => '2'], $copy->getQueryParams());
        $this->assertSame($request, $request->setServerParams(['REQUEST_METHOD' => 'GET'])
            ->setCookieParams(['session' => 'changed'])->setQueryParams(['page' => '1']));
        $this->assertSame(['REQUEST_METHOD' => 'GET'], $request->getServerParams());
        $this->assertSame(['session' => 'changed'], $request->getCookieParams());
        $this->assertSame(['page' => '1'], $request->getQueryParams());
    }

    public function testAttributesPreserveNullAndCopyIsolation(): void
    {
        $request = new ServerRequest();
        $this->assertSame('fallback', $request->getAttribute('missing', 'fallback'));
        $this->assertSame($request, $request->withoutAttribute('missing'));
        $request->setAttribute('nullable', null);
        $this->assertNull($request->getAttribute('nullable', 'fallback'));
        $copy = $request->withAttribute('id', 42);
        $this->assertSame(['nullable' => null], $request->getAttributes());
        $this->assertSame(['nullable' => null, 'id' => 42], $copy->getAttributes());
        $removed = $copy->withoutAttribute('id');
        $this->assertSame(42, $copy->getAttribute('id'));
        $this->assertNull($removed->getAttribute('id'));
        $this->assertSame($request, $request->unsetAttribute('nullable'));
        $this->assertSame([], $request->getAttributes());
    }

    public function testParsedBodyAndUploadedFiles(): void
    {
        $request = new ServerRequest();
        $request->setHeader('Content-Type', 'application/json');
        $request->setBody(Psr7::createStream('{"name":"reader"}'));
        $this->assertSame('application/json', $request->getContentType());
        $this->assertSame(strlen('{"name":"reader"}'), $request->getContentLength());
        $this->assertSame(['name' => 'reader'], $request->getParsedBody());
        $this->assertSame(['name' => 'reader'], $request->getParsedBody());
        $copy = $request->withParsedBody(null);
        $this->assertNull($copy->getParsedBody());
        $this->assertSame(['name' => 'reader'], $request->getParsedBody());
        $body = (object) ['name' => 'manual'];
        $this->assertSame($request, $request->setParsedBody($body));
        $this->assertSame($body, $request->getParsedBody());

        $file = new UploadedFile(Psr7::createStream('data'), 4);
        $files = $request->withUploadedFiles(['document' => $file]);
        $this->assertSame([], $request->getUploadedFiles());
        $this->assertSame(['document' => $file], $files->getUploadedFiles());
        $this->assertSame($request, $request->setUploadedFiles(['document' => $file]));
        $this->assertSame(['document' => $file], $request->getUploadedFiles());
        $this->assertNull($request->isUpgrade());
        $this->assertSame($request, $request->setIsUpgrade(true));
        $this->assertTrue($request->isUpgrade());
    }
}
