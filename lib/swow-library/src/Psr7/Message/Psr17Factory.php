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

namespace Swow\Psr7\Message;

use Psr\Http\Message\RequestFactoryInterface;
use Psr\Http\Message\ResponseFactoryInterface;
use Psr\Http\Message\ServerRequestFactoryInterface;
use Psr\Http\Message\ServerRequestInterface;
use Psr\Http\Message\StreamFactoryInterface;
use Psr\Http\Message\StreamInterface;
use Psr\Http\Message\UploadedFileFactoryInterface;
use Psr\Http\Message\UriFactoryInterface;
use RuntimeException;
use Swow\Buffer;
use Swow\Http\Status;
use Swow\Psr7\Server\Server;
use Swow\Psr7\Server\ServerConnection;
use Swow\Psr7\Server\ServerConnectionFactoryInterface;

use function error_get_last;
use function fopen;
use function sprintf;
use function strlen;

use const UPLOAD_ERR_OK;

class Psr17Factory implements UriFactoryInterface, StreamFactoryInterface, RequestFactoryInterface, ResponseFactoryInterface, ServerRequestFactoryInterface, UploadedFileFactoryInterface, ServerConnectionFactoryInterface
{
    protected static self $instance;

    public static function getInstance(): static
    {
        return self::$instance ??= new static();
    }

    public function createUri(string $uri = ''): Uri
    {
        return new Uri($uri);
    }

    public function createStream(string $content = ''): BufferStream
    {
        $buffer = new Buffer(strlen($content));
        if ($content !== '') {
            $buffer->append($content);
        }
        return $this->createStreamFromBuffer($buffer);
    }

    public function createStreamFromBuffer(Buffer $buffer): BufferStream
    {
        return new BufferStream($buffer);
    }

    public function createStreamFromFile(string $filename, string $mode = 'rb'): PhpStream
    {
        $resource = @fopen($filename, $mode);
        if ($resource === false) {
            throw new RuntimeException(sprintf('The file "%s" cannot be opened with %s mode, reason: %s', $filename, $mode, error_get_last()['message'] ?? 'unknown error'));
        }

        return new PhpStream($resource);
    }

    public function createStreamFromResource($resource): PhpStream
    {
        return new PhpStream($resource);
    }

    public function createRequest(string $method, $uri): Request
    {
        return (new Request())->setMethod($method)->setUri($uri);
    }

    public function createResponse(
        int $code = Status::OK,
        string $reasonPhrase = ''
    ): Response {
        return (new Response())->setStatus($code, $reasonPhrase);
    }

    /**
     * @param array<string, string> $serverParams
     */
    public function createServerRequest(string $method, mixed $uri, array $serverParams = []): ServerRequestInterface
    {
        return (new ServerRequest())->setMethod($method)->setUri($uri)->setServerParams($serverParams);
    }

    public function createUploadedFile(
        StreamInterface $stream,
        ?int $size = null,
        int $error = UPLOAD_ERR_OK,
        ?string $clientFilename = null,
        ?string $clientMediaType = null
    ): UploadedFile {
        return new UploadedFile($stream, $size, $error, $clientFilename, $clientMediaType);
    }

    public function createServerConnection(Server $server): ServerConnection
    {
        return new ServerConnection($server);
    }
}
