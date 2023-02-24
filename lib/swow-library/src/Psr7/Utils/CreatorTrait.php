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

use Psr\Http\Message\RequestFactoryInterface;
use Psr\Http\Message\RequestInterface;
use Psr\Http\Message\ResponseFactoryInterface;
use Psr\Http\Message\ResponseInterface;
use Psr\Http\Message\ServerRequestFactoryInterface;
use Psr\Http\Message\ServerRequestInterface;
use Psr\Http\Message\StreamFactoryInterface;
use Psr\Http\Message\StreamInterface;
use Psr\Http\Message\UploadedFileFactoryInterface;
use Psr\Http\Message\UploadedFileInterface;
use Psr\Http\Message\UriFactoryInterface;
use Psr\Http\Message\UriInterface;
use Swow\Buffer;
use Swow\Http\Message\ResponseEntity;
use Swow\Http\Message\ServerRequestEntity;
use Swow\Http\Status;
use Swow\Psr7\Message\BufferStream;
use Swow\Psr7\Message\PhpStream;
use Swow\Psr7\Message\Psr17Factory;
use Swow\Psr7\Message\Request;
use Swow\Psr7\Message\Response;
use Swow\Psr7\Message\ResponsePlusInterface;
use Swow\Psr7\Message\ServerRequest;
use Swow\Psr7\Message\ServerRequestPlusInterface;
use Swow\Psr7\Message\StreamPlusInterface;
use Swow\Psr7\Message\UploadedFile;
use Swow\Psr7\Message\UploadedFilePlusInterface;
use Swow\Psr7\Message\Uri;
use Swow\Psr7\Message\UriPlusInterface;
use Swow\Psr7\Message\WebSocketFrame;
use Swow\WebSocket\Opcode;
use Swow\WebSocket\WebSocket;

use function is_resource;
use function is_string;
use function parse_str;

trait CreatorTrait
{
    /**
     * @return UriInterface|UriPlusInterface|Uri
     */
    public static function createUriFromString(string $uri, ?UriFactoryInterface $uriFactory = null): UriInterface
    {
        $uriFactory ??= static::getDefaultUriFactory();
        return $uriFactory->createUri($uri);
    }

    /**
     * @return StreamInterface|StreamPlusInterface|BufferStream
     */
    public static function createStream(string $data = '', ?StreamFactoryInterface $streamFactory = null): StreamInterface
    {
        return ($streamFactory ?? static::getDefaultStreamFactory())->createStream($data);
    }

    /**
     * @return StreamInterface|StreamPlusInterface|BufferStream
     */
    public static function createStreamFromBuffer(Buffer $buffer, ?StreamFactoryInterface $streamFactory = null): StreamInterface
    {
        $streamFactory ??= static::getDefaultStreamFactory();
        if ($streamFactory instanceof Psr17Factory) {
            return $streamFactory->createStreamFromBuffer($buffer);
        } else {
            return $streamFactory->createStream((string) $buffer);
        }
    }

    /**
     * @return StreamInterface|StreamPlusInterface|BufferStream|PhpStream
     */
    public static function createStreamFromAny(mixed $data = '', ?StreamFactoryInterface $streamFactory = null): StreamInterface
    {
        if ($data instanceof StreamInterface) {
            return $data;
        }
        $streamFactory ??= static::getDefaultStreamFactory();
        if (is_resource($data)) {
            return $streamFactory->createStreamFromResource($data);
        }
        if ($data instanceof Buffer && $streamFactory instanceof Psr17Factory) {
            return $streamFactory->createStreamFromBuffer($data);
        } else {
            return $streamFactory->createStream((string) $data);
        }
    }

    /**
     * @param array<string, array<string>|string> $headers
     * @return RequestInterface|Request
     */
    public static function createRequest(
        string $method,
        mixed $uri,
        array $headers = [],
        mixed $body = null,
        ?RequestFactoryInterface $requestFactory = null
    ): RequestInterface {
        $requestFactory ??= static::getDefaultRequestFactory();
        $request = $requestFactory->createRequest($method, $uri);
        if ($headers) {
            $request = static::setHeaders($request, $headers);
        }
        if ($body) {
            $request = static::setBody($request, $body);
        }
        return $request;
    }

    /**
     * @param array<string, array<string>|string> $headers
     * @return ResponseInterface|ResponsePlusInterface|Response
     */
    public static function createResponse(
        int $code = Status::OK,
        string $reasonPhrase = '',
        array $headers = [],
        mixed $body = null,
        ?ResponseFactoryInterface $responseFactory = null
    ): ResponseInterface {
        $responseFactory ??= static::getDefaultResponseFactory();
        $response = $responseFactory->createResponse($code, $reasonPhrase);
        if ($headers) {
            $response = static::setHeaders($response, $headers);
        }
        if ($body) {
            $response = static::setBody($response, $body);
        }
        return $response;
    }

    /**
     * @return ResponseInterface|ResponsePlusInterface|Response
     */
    public static function createResponseFromEntity(ResponseEntity $responseEntity, ?ResponseFactoryInterface $responseFactory = null, ?StreamFactoryInterface $streamFactory = null): ResponseInterface
    {
        $responseFactory ??= static::getDefaultResponseFactory();
        $body = $responseEntity->body;
        if ($body) {
            $bodyStream = static::createStreamFromBuffer($body, $streamFactory);
        } else {
            $bodyStream = null;
        }
        $response = $responseFactory->createResponse(
            $responseEntity->statusCode,
            $responseEntity->reasonPhrase
        );
        if ($response instanceof ResponsePlusInterface) {
            $response
                ->setProtocolVersion($responseEntity->protocolVersion)
                ->setHeadersAndHeaderNames(
                    $responseEntity->headers,
                    $responseEntity->headerNames
                );
            if ($bodyStream) {
                $response->setBody($bodyStream);
            }
            if ($response instanceof Response) {
                if (!$responseEntity->shouldKeepAlive) {
                    $response->setShouldKeepAlive(false);
                }
            }
        } else {
            $response = $response->withProtocolVersion($responseEntity->protocolVersion);
            foreach ($responseEntity->headers as $headerName => $headerValue) {
                $response = $response->withHeader($headerName, $headerValue);
            }
            if ($bodyStream) {
                $response = $response->withBody($bodyStream);
            }
        }
        return $response;
    }

    /**
     * @param array<\Swow\Http\Message\UploadedFileEntity> $uploadedFileEntities
     * @return array<UploadedFileInterface|UploadedFilePlusInterface|UploadedFile>
     */
    public static function createUploadedFilesFromEntity(array $uploadedFileEntities, ?StreamFactoryInterface $streamFactory = null, ?UploadedFileFactoryInterface $uploadedFileFactory = null): array
    {
        $streamFactory ??= static::getDefaultStreamFactory();
        $uploadedFileFactory ??= static::getDefaultUploadedFileFactory();
        $uploadedFiles = [];
        foreach ($uploadedFileEntities as $formDataName => $uploadedFileEntity) {
            $uploadedFileStream = $streamFactory->createStreamFromResource($uploadedFileEntity->tmpFile);
            $uploadedFiles[$formDataName] = $uploadedFileFactory->createUploadedFile(
                $uploadedFileStream,
                $uploadedFileEntity->size,
                $uploadedFileEntity->error,
                $uploadedFileEntity->name,
                $uploadedFileEntity->type,
            );
        }
        return $uploadedFiles;
    }

    /**
     * @param array<string, array<string>|string> $headers
     * @param array<string, string> $serverParams
     * @return ServerRequestInterface|ServerRequestPlusInterface|ServerRequest
     */
    public static function createServerRequest(
        string $method,
        mixed $uri,
        array $serverParams,
        array $headers = [],
        mixed $body = null,
        ?ServerRequestFactoryInterface $serverRequestFactory = null
    ): ServerRequestInterface {
        $serverRequestFactory ??= static::getDefaultServerRequestFactory();
        $serverRequest = $serverRequestFactory->createServerRequest($method, $uri, $serverParams);
        if ($headers) {
            $serverRequest = static::setHeaders($serverRequest, $headers);
        }
        if ($body) {
            $serverRequest = static::setBody($serverRequest, $body);
        }
        return $serverRequest;
    }

    /**
     * @return ServerRequestInterface|ServerRequestPlusInterface|ServerRequest
     */
    public static function createServerRequestFromEntity(
        ServerRequestEntity $serverRequestEntity,
        ?ServerRequestFactoryInterface $serverRequestFactory = null,
        ?UriFactoryInterface $uriFactory = null,
        ?StreamFactoryInterface $streamFactory = null,
        ?UploadedFileFactoryInterface $uploadedFileFactory = null
    ): ServerRequestInterface {
        $serverRequestFactory ??= static::getDefaultServerRequestFactory();
        $serverRequest = $serverRequestFactory->createServerRequest(
            $serverRequestEntity->method,
            static::createUriFromString($serverRequestEntity->uri, $uriFactory),
            $serverRequestEntity->serverParams
        );
        $query = $serverRequest->getUri()->getQuery();
        if ($query) {
            parse_str($query, $queryParams);
        } else {
            $queryParams = [];
        }
        if ($serverRequestEntity->body) {
            $bodyStream = $streamFactory->createStream((string) $serverRequestEntity->body);
        } else {
            $bodyStream = null;
        }
        if ($serverRequestEntity->uploadedFiles) {
            $uploadedFiles = static::createUploadedFilesFromEntity(
                $serverRequestEntity->uploadedFiles,
                $streamFactory, $uploadedFileFactory
            );
        } else {
            $uploadedFiles = null;
        }
        if ($serverRequest instanceof ServerRequestPlusInterface) {
            if ($queryParams) {
                $serverRequest->setQueryParams($queryParams);
            }
            $serverRequest->setHeadersAndHeaderNames(
                $serverRequestEntity->headers,
                $serverRequestEntity->headerNames
            );
            if ($serverRequestEntity->cookies) {
                $serverRequest->setCookieParams($serverRequestEntity->cookies);
            }
            if ($bodyStream) {
                $serverRequest->setBody($bodyStream);
            }
            if ($serverRequestEntity->formData) {
                $serverRequest->setParsedBody($serverRequestEntity->formData);
            }
            if ($uploadedFiles) {
                $serverRequest->setUploadedFiles($uploadedFiles);
            }
            if ($serverRequest instanceof ServerRequest) {
                $serverRequest->setShouldKeepAlive($serverRequestEntity->shouldKeepAlive);
                if ($serverRequestEntity->isUpgrade) {
                    $serverRequest->setIsUpgrade(true);
                }
            }
        } else {
            if ($queryParams) {
                $serverRequest = $serverRequest->withQueryParams($queryParams);
            }
            foreach ($serverRequestEntity->headers as $headerName => $headerValue) {
                $serverRequest = $serverRequest->withHeader($headerName, $headerValue);
            }
            if ($serverRequestEntity->cookies) {
                $serverRequest = $serverRequest->withCookieParams($serverRequestEntity->cookies);
            }
            if ($bodyStream) {
                $serverRequest = $serverRequest->withBody($bodyStream);
            }
            if ($serverRequestEntity->formData) {
                $serverRequest = $serverRequest->withParsedBody($serverRequestEntity->formData);
            }
            if ($uploadedFiles) {
                $serverRequest = $serverRequest->withUploadedFiles($uploadedFiles);
            }
        }
        return $serverRequest;
    }

    public static function createWebSocketFrame(int $opcode, mixed $payloadData, bool $fin = true, bool|string $mask = false): WebSocketFrame
    {
        if ($mask) {
            $maskingKey = is_string($mask) ? $mask : WebSocket::DEFAULT_MASKING_KEY;
            $payloadData = WebSocket::mask((string) $payloadData, maskingKey: $maskingKey);
        } else {
            $maskingKey = '';
        }
        return new WebSocketFrame(fin: $fin, opcode: $opcode, maskingKey: $maskingKey, payloadData: $payloadData);
    }

    public static function createWebSocketTextFrame(mixed $payloadData, bool $fin = true, bool|string $mask = false): WebSocketFrame
    {
        return static::createWebSocketFrame(opcode: Opcode::TEXT, payloadData: $payloadData, fin: $fin, mask: $mask);
    }

    public static function createWebSocketTextMaskedFrame(mixed $payloadData, bool $fin = true): WebSocketFrame
    {
        return static::createWebSocketFrame(opcode: Opcode::TEXT, payloadData: $payloadData, fin: $fin, mask: true);
    }

    public static function createWebSocketBinaryFrame(mixed $payloadData, bool $fin = true, bool|string $mask = false): WebSocketFrame
    {
        return static::createWebSocketFrame(opcode: Opcode::BINARY, payloadData: $payloadData, fin: $fin, mask: $mask);
    }

    public static function createWebSocketBinaryMaskedFrame(mixed $payloadData, bool $fin = true): WebSocketFrame
    {
        return static::createWebSocketFrame(opcode: Opcode::BINARY, payloadData: $payloadData, fin: $fin, mask: true);
    }
}
