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

namespace Swow\Tests\Psr7\Utils;

use PHPUnit\Framework\Attributes\CoversClass;
use PHPUnit\Framework\TestCase;
use Swow\Buffer;
use Swow\Http\Message\ResponseEntity;
use Swow\Http\Message\ServerRequestEntity;
use Swow\Psr7\Message\Psr17Factory;
use Swow\Psr7\Message\Response;
use Swow\Psr7\Message\ServerRequest;
use Swow\Psr7\Psr7;

/** @internal */
#[CoversClass(Psr7::class)]
#[CoversClass(Psr17Factory::class)]
#[CoversClass(Response::class)]
#[CoversClass(ServerRequest::class)]
final class CreatorTest extends TestCase
{
    public function testStreamsKeepTheirOriginalResourceOrBuffer(): void
    {
        $buffer = new Buffer(0);
        $buffer->append('buffer');
        $stream = Psr7::createStreamFromBuffer($buffer);
        $buffer->append(' shared');
        $this->assertSame('buffer shared', (string) $stream);
        $this->assertSame($stream, Psr7::createStreamFromAny($stream));
        $this->assertSame('buffer shared', (string) Psr7::createStreamFromAny($buffer));
        $resource = fopen('php://temp', 'w+b');
        fwrite($resource, 'resource');
        rewind($resource);
        $resourceStream = Psr7::createStreamFromAny($resource);
        try {
            $this->assertSame('resource', (string) $resourceStream);
            $this->assertSame('scalar', (string) Psr7::createStreamFromAny('scalar'));
        } finally {
            $resourceStream->close();
        }
    }

    public function testResponseEntityConversionPreservesProtocolAndBody(): void
    {
        $entity = new ResponseEntity();
        $entity->protocolVersion = '1.0';
        $entity->statusCode = 201;
        $entity->reasonPhrase = 'Created';
        $entity->headers = ['X-Result' => ['one', 'two']];
        $entity->headerNames = ['x-result' => 'X-Result'];
        $entity->body = new Buffer(0);
        $entity->body->append('result');
        $entity->shouldKeepAlive = false;
        $response = Psr7::createResponseFromEntity($entity);
        $this->assertSame('1.0', $response->getProtocolVersion());
        $this->assertSame(201, $response->getStatusCode());
        $this->assertSame('Created', $response->getReasonPhrase());
        $this->assertSame('one, two', $response->getHeaderLine('X-Result'));
        $this->assertSame('result', (string) $response->getBody());
        $this->assertFalse($response->shouldKeepAlive());
        $vector = Psr7::convertResponseToVector($response);
        $this->assertSame('result', $vector[1]);
        $this->assertSame(Psr7::stringifyResponse($response), implode('', $vector));
        $this->assertSame($vector[0], Psr7::stringifyResponse($response, true));
    }

    public function testServerRequestEntityConversion(): void
    {
        $entity = new ServerRequestEntity();
        $entity->method = 'POST';
        $entity->uri = '/form?name=reader';
        $entity->headers = ['Content-Type' => ['application/x-www-form-urlencoded']];
        $entity->headerNames = ['content-type' => 'Content-Type'];
        $entity->body = new Buffer(0);
        $entity->body->append('value=submitted');
        $entity->cookies = ['session' => 'cookie'];
        $entity->formData = ['value' => 'submitted'];
        $entity->serverParams = ['remote_addr' => '127.0.0.1', 'remote_port' => 12345];
        $entity->shouldKeepAlive = true;
        $entity->isUpgrade = true;
        $request = Psr7::createServerRequestFromEntity($entity);
        $this->assertSame('POST', $request->getMethod());
        $this->assertSame('/form?name=reader', $request->getRequestTarget());
        $this->assertSame(['name' => 'reader'], $request->getQueryParams());
        $this->assertSame(['session' => 'cookie'], $request->getCookieParams());
        $this->assertSame(['value' => 'submitted'], $request->getParsedBody());
        $this->assertSame($entity->serverParams, $request->getServerParams());
        $this->assertTrue($request->shouldKeepAlive());
        $this->assertTrue($request->isUpgrade());
        $this->assertSame('value=submitted', (string) $request->getBody());
        $this->assertSame($request->toString(), Psr7::stringifyRequest($request));
        $this->assertSame($request->toString(true), Psr7::stringifyRequest($request, true));
    }

    public function testZeroStringBodyIsNotDiscarded(): void
    {
        $this->assertSame('0', (string) Psr7::createRequest('POST', '/', body: '0')->getBody());
        $this->assertSame('0', (string) Psr7::createResponse(body: '0')->getBody());
        $this->assertSame('0', (string) Psr7::createServerRequest('POST', '/', [], body: '0')->getBody());
    }
}
