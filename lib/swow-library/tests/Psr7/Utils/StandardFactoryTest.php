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

use Nyholm\Psr7\Factory\Psr17Factory as NyholmFactory;
use Nyholm\Psr7\Request as NyholmRequest;
use Nyholm\Psr7\Response as NyholmResponse;
use Nyholm\Psr7\ServerRequest as NyholmServerRequest;
use Nyholm\Psr7\Stream as NyholmStream;
use Nyholm\Psr7\UploadedFile as NyholmUploadedFile;
use Nyholm\Psr7\Uri as NyholmUri;
use PHPUnit\Framework\Attributes\CoversClass;
use PHPUnit\Framework\TestCase;
use Swow\Buffer;
use Swow\Http\Message\ResponseEntity;
use Swow\Http\Message\ServerRequestEntity;
use Swow\Http\Message\UploadedFileEntity;
use Swow\Psr7\Message\ServerRequest;
use Swow\Psr7\Psr7;

use function is_resource;

use const UPLOAD_ERR_OK;

/** @internal */
#[CoversClass(Psr7::class)]
#[CoversClass(ServerRequest::class)]
final class StandardFactoryTest extends TestCase
{
    public function testExplicitFactoriesCreateStandardMessagesAndStreams(): void
    {
        $factory = new NyholmFactory();
        $uri = Psr7::createUriFromString('https://example.test/path', $factory);
        $this->assertInstanceOf(NyholmUri::class, $uri);
        $request = Psr7::createRequest('POST', $uri, ['X-Request' => ['one', 'two']], 'body', $factory);
        $this->assertInstanceOf(NyholmRequest::class, $request);
        $this->assertSame('POST', $request->getMethod());
        $this->assertSame('one, two', $request->getHeaderLine('X-Request'));
        $this->assertSame('body', (string) $request->getBody());
        $response = Psr7::createResponse(201, 'Stored', ['X-Response' => 'value'], '0', $factory);
        $this->assertInstanceOf(NyholmResponse::class, $response);
        $this->assertSame(201, $response->getStatusCode());
        $this->assertSame('Stored', $response->getReasonPhrase());
        $this->assertSame('value', $response->getHeaderLine('X-Response'));
        $this->assertSame('0', (string) $response->getBody());
        $serverRequest = Psr7::createServerRequest('POST', '/form', ['remote_addr' => '127.0.0.1'], ['X-Form' => 'value'], 'submitted', $factory);
        $this->assertInstanceOf(NyholmServerRequest::class, $serverRequest);
        $this->assertSame(['remote_addr' => '127.0.0.1'], $serverRequest->getServerParams());
        $this->assertSame('value', $serverRequest->getHeaderLine('X-Form'));
        $this->assertSame('submitted', (string) $serverRequest->getBody());

        $buffer = new Buffer(0);
        $buffer->append("a\0b");
        $stream = Psr7::createStreamFromBuffer($buffer, $factory);
        $this->assertInstanceOf(NyholmStream::class, $stream);
        $buffer->append('later');
        $this->assertSame("a\0b", (string) $stream);
        $this->assertSame("a\0blater", (string) Psr7::createStreamFromAny($buffer, $factory));
        $this->assertInstanceOf(NyholmStream::class, Psr7::createStream('value', $factory));
    }

    public function testResponseEntityConversionUsesImmutableFactoryMethods(): void
    {
        $entity = new ResponseEntity();
        $entity->protocolVersion = '1.0';
        $entity->statusCode = 202;
        $entity->reasonPhrase = 'Queued';
        $entity->headers = ['X-Result' => ['one', 'two']];
        $entity->body = new Buffer(0);
        $entity->body->append("entity\0body");
        $factory = new NyholmFactory();
        $response = Psr7::createResponseFromEntity($entity, $factory, $factory);
        $this->assertInstanceOf(NyholmResponse::class, $response);
        $this->assertSame('1.0', $response->getProtocolVersion());
        $this->assertSame(202, $response->getStatusCode());
        $this->assertSame('Queued', $response->getReasonPhrase());
        $this->assertSame(['one', 'two'], $response->getHeader('X-Result'));
        $this->assertSame("entity\0body", (string) $response->getBody());
        $entity->body->append('changed');
        $this->assertSame("entity\0body", (string) $response->getBody());
        $entity->body = null;
        $this->assertSame('', (string) Psr7::createResponseFromEntity($entity, $factory, $factory)->getBody());
    }

    public function testServerEntityConversionPreservesFieldsAndUploadResources(): void
    {
        $resource = tmpfile();
        $this->assertIsResource($resource);
        fwrite($resource, "file\0data");
        rewind($resource);
        $file = new UploadedFileEntity();
        $file->name = 'data.bin';
        $file->type = 'application/octet-stream';
        $file->tmpName = stream_get_meta_data($resource)['uri'];
        $file->tmpFile = $resource;
        $file->size = 9;
        $file->error = UPLOAD_ERR_OK;
        $entity = new ServerRequestEntity();
        $entity->method = 'POST';
        $entity->uri = '/form?page=2';
        $entity->serverParams = ['remote_addr' => '127.0.0.1', 'remote_port' => 8080];
        $entity->headers = ['X-Form' => ['one', 'two']];
        $entity->cookies = ['session' => 'cookie'];
        $entity->formData = ['name' => 'value'];
        $entity->uploadedFiles = ['upload' => $file];
        $entity->body = new Buffer(0);
        $entity->body->append('wire body');
        $factory = new NyholmFactory();
        try {
            $request = Psr7::createServerRequestFromEntity($entity, $factory, $factory, $factory, $factory);
            $this->assertInstanceOf(NyholmServerRequest::class, $request);
            $this->assertSame('POST', $request->getMethod());
            $this->assertSame(['page' => '2'], $request->getQueryParams());
            $this->assertSame($entity->serverParams, $request->getServerParams());
            $this->assertSame(['one', 'two'], $request->getHeader('X-Form'));
            $this->assertSame($entity->cookies, $request->getCookieParams());
            $this->assertSame($entity->formData, $request->getParsedBody());
            $this->assertSame('wire body', (string) $request->getBody());
            $upload = $request->getUploadedFiles()['upload'];
            $this->assertSame('data.bin', $upload->getClientFilename());
            $this->assertSame('application/octet-stream', $upload->getClientMediaType());
            $this->assertSame(9, $upload->getSize());
            $this->assertSame(UPLOAD_ERR_OK, $upload->getError());
            $this->assertSame("file\0data", (string) $upload->getStream());
        } finally {
            if (is_resource($resource)) {
                fclose($resource);
            }
        }
    }

    public function testConfiguredDefaultFactoriesAreUsedAndCanBeRestored(): void
    {
        $factory = new NyholmFactory();
        $kinds = ['Uri', 'Stream', 'Request', 'Response', 'ServerRequest', 'UploadedFile'];
        $defaults = [];
        foreach ($kinds as $kind) {
            $defaults[$kind] = Psr7::{'getDefault' . $kind . 'Factory'}();
        }
        $resource = tmpfile();
        $this->assertIsResource($resource);
        try {
            foreach ($kinds as $kind) {
                Psr7::{'setDefault' . $kind . 'Factory'}($factory);
                $this->assertSame($factory, Psr7::{'getDefault' . $kind . 'Factory'}());
            }
            $this->assertInstanceOf(NyholmUri::class, Psr7::createUriFromString('/default'));
            $this->assertInstanceOf(NyholmStream::class, Psr7::createStream('default'));
            $this->assertInstanceOf(NyholmRequest::class, Psr7::createRequest('GET', '/default'));
            $this->assertInstanceOf(NyholmResponse::class, Psr7::createResponse());
            $this->assertInstanceOf(NyholmServerRequest::class, Psr7::createServerRequest('GET', '/default', []));
            $entity = new UploadedFileEntity();
            $entity->tmpFile = $resource;
            $entity->size = 0;
            $entity->error = UPLOAD_ERR_OK;
            $entity->name = 'empty.bin';
            $entity->type = 'application/octet-stream';
            $files = Psr7::createUploadedFilesFromEntity(['empty' => $entity]);
            $this->assertInstanceOf(NyholmUploadedFile::class, $files['empty']);
            $this->assertSame(0, $files['empty']->getSize());
            $this->assertSame('', (string) $files['empty']->getStream());
        } finally {
            foreach ($defaults as $kind => $default) {
                Psr7::{'setDefault' . $kind . 'Factory'}($default);
            }
            if (is_resource($resource)) {
                fclose($resource);
            }
        }
        foreach ($defaults as $kind => $default) {
            $this->assertSame($default, Psr7::{'getDefault' . $kind . 'Factory'}());
        }
    }

    public function testServerEntityProtocolVersionSurvivesBothFactoryPaths(): void
    {
        $entity = new ServerRequestEntity();
        $entity->method = 'GET';
        $entity->uri = '/old-protocol';
        $entity->protocolVersion = '1.0';
        foreach ([Psr7::getBuiltinPsr17Factory(), new NyholmFactory()] as $factory) {
            $request = Psr7::createServerRequestFromEntity($entity, $factory);
            $this->assertSame('1.0', $request->getProtocolVersion());
            $this->assertSame('GET', $request->getMethod());
            $this->assertSame('/old-protocol', $request->getRequestTarget());
        }
    }
}
