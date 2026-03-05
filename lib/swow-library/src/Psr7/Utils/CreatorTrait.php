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

use Generator;
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
use Swow\Http\Message\UploadedFileEntity;
use Swow\Http\Protocol\ChunkedBodyStream;
use Swow\Http\Status;
use Swow\Psr7\Message\BufferStream;
use Swow\Psr7\Message\ChunkedBodyPsrStream;
use Swow\Psr7\Message\EventStreamDecoder;
use Swow\Psr7\Message\EventStreamEvent;
use Swow\Psr7\Message\MessagePlusInterface;
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

use function array_filter;
use function array_map;
use function explode;
use function implode;
use function is_resource;
use function is_string;
use function parse_str;
use function strcasecmp;
use function trim;

trait CreatorTrait
{
    /* 这里只处理 plus 对象：它支持原地改 header。
     * 非 plus 对象通常是不可变语义（withHeader 返回新实例），
     * 而 chunked 完成事件发生在对象已返回之后，无法安全替换调用方持有的实例。 */
    /**
     * 在 chunked 流式 body 完成后，把 header 归一化为“普通完整 body”形态：
     * - 移除 Transfer-Encoding 中的 chunked；
     * - 写入最终 Content-Length。
     */
    protected static function normalizeChunkedHeadersForPlusMessage(MessagePlusInterface $message, int $contentLength): void
    {
        if ($message->hasHeader('transfer-encoding')) {
            $transferEncoding = implode(', ', $message->getHeader('transfer-encoding'));
            $transferEncodings = array_filter(
                array_map('trim', explode(',', $transferEncoding)),
                static function (string $value): bool {
                    return $value !== '' && strcasecmp($value, 'chunked') !== 0;
                }
            );
            if ($transferEncodings === []) {
                $message->unsetHeader('Transfer-Encoding');
            } else {
                $message->setHeader('Transfer-Encoding', implode(', ', $transferEncodings));
            }
        }
        $message->setHeader('Content-Length', (string) $contentLength);
    }

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
        if ($data instanceof ChunkedBodyStream) {
            // 分层边界：HTTP 层流对象在这里适配成 PSR7 StreamInterface。
            return new ChunkedBodyPsrStream($data);
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
     * 从 PSR Stream 按 SSE 协议读取事件流。
     *
     * @return Generator<int, EventStreamEvent>
     */
    public static function readEventStream(StreamInterface $stream, int $readSize = 8192): Generator
    {
        return EventStreamDecoder::decode($stream, $readSize);
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
        ?RequestFactoryInterface $requestFactory = null,
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
        ?ResponseFactoryInterface $responseFactory = null,
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
        $streamFactory ??= static::getDefaultStreamFactory();
        $rawBody = $responseEntity->body;
        if ($responseEntity->body !== null) {
            $bodyStream = static::createStreamFromAny($rawBody, $streamFactory);
        } else {
            $bodyStream = null;
        }
        $response = $responseFactory->createResponse(
            $responseEntity->statusCode,
            $responseEntity->reasonPhrase
        );
        if ($response instanceof ResponsePlusInterface) {
            $response->setProtocolVersion($responseEntity->protocolVersion);
            if (method_exists($response, 'setHeadersAndHeaderNames')) {
                $response->{'setHeadersAndHeaderNames'}(
                    $responseEntity->headers,
                    $responseEntity->headerNames
                );
            } else {
                $response->setHeaders($responseEntity->headers);
            }
            if ($bodyStream) {
                $response->setBody($bodyStream);
                if ($rawBody instanceof ChunkedBodyStream) {
                    // 完成后再归一化 header，避免在 body 未完整消费时伪造 Content-Length。
                    $rawBody->onCompleted(static function () use ($response, $rawBody): void {
                        self::normalizeChunkedHeadersForPlusMessage($response, $rawBody->getSize() ?? 0);
                    });
                }
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
     * @param array<UploadedFileEntity> $uploadedFileEntities
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
        ?ServerRequestFactoryInterface $serverRequestFactory = null,
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
        ?UploadedFileFactoryInterface $uploadedFileFactory = null,
    ): ServerRequestInterface {
        $serverRequestFactory ??= static::getDefaultServerRequestFactory();
        $streamFactory ??= static::getDefaultStreamFactory();
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
        $rawBody = $serverRequestEntity->body;
        if ($serverRequestEntity->body !== null) {
            $bodyStream = static::createStreamFromAny($rawBody, $streamFactory);
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
            if (method_exists($serverRequest, 'setHeadersAndHeaderNames')) {
                $serverRequest->{'setHeadersAndHeaderNames'}(
                    $serverRequestEntity->headers,
                    $serverRequestEntity->headerNames
                );
            } else {
                $serverRequest->setHeaders($serverRequestEntity->headers);
            }
            if ($serverRequestEntity->cookies) {
                $serverRequest->setCookieParams($serverRequestEntity->cookies);
            }
            if ($bodyStream) {
                $serverRequest->setBody($bodyStream);
                if ($rawBody instanceof ChunkedBodyStream) {
                    // 与 response 分支一致：仅在流完成后回写长度相关 header。
                    $rawBody->onCompleted(static function () use ($serverRequest, $rawBody): void {
                        self::normalizeChunkedHeadersForPlusMessage($serverRequest, $rawBody->getSize() ?? 0);
                    });
                }
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
