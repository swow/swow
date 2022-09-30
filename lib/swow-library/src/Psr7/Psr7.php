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

namespace Swow\Psr7;

use Psr\Http\Message\MessageInterface;
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
use Swow\Http\Http;
use Swow\Http\Message\ResponseEntity;
use Swow\Http\Message\ServerRequestEntity;
use Swow\Http\Status;
use Swow\Psr7\Message\BufferStream;
use Swow\Psr7\Message\MessagePlusInterface;
use Swow\Psr7\Message\PhpStream;
use Swow\Psr7\Message\Psr17Factory;
use Swow\Psr7\Message\Psr17PlusFactoryInterface;
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

use function is_resource;
use function parse_str;

class Psr7
{
    protected static Psr17PlusFactoryInterface $psr17Factory;

    public static function getDefaultPsr17Factory(): Psr17PlusFactoryInterface
    {
        return self::$psr17Factory ??= new Psr17Factory();
    }

    /**
     * @return UriInterface|UriPlusInterface|Uri
     */
    public static function createUriFromString(string $uri, ?UriFactoryInterface $uriFactory = null): UriInterface
    {
        $uriFactory ??= static::getDefaultPsr17Factory();
        return $uriFactory->createUri($uri);
    }

    /**
     * @return StreamInterface|StreamPlusInterface|BufferStream
     */
    public static function createStream(string $data = '', ?StreamFactoryInterface $streamFactory = null): StreamInterface
    {
        return ($streamFactory ?? static::getDefaultPsr17Factory())->createStream($data);
    }

    /**
     * @return StreamInterface|StreamPlusInterface|BufferStream
     */
    public static function createStreamFromBuffer(Buffer $buffer, ?StreamFactoryInterface $streamFactory = null): StreamInterface
    {
        $streamFactory ??= static::getDefaultPsr17Factory();
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
        $streamFactory ??= static::getDefaultPsr17Factory();
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
        $requestFactory ??= static::getDefaultPsr17Factory();
        $request = $requestFactory->createRequest($method, $uri);
        if ($headers) {
            static::setHeaders($request, $headers);
        }
        if ($body) {
            static::setBody($request, $body);
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
        $responseFactory ??= static::getDefaultPsr17Factory();
        $response = $responseFactory->createResponse($code, $reasonPhrase);
        if ($headers) {
            static::setHeaders($response, $headers);
        }
        if ($body) {
            static::setBody($response, $body);
        }
        return $response;
    }

    /**
     * @return ResponseInterface|ResponsePlusInterface|Response
     */
    public static function createResponseFromEntity(ResponseEntity $responseEntity, ?ResponseFactoryInterface $responseFactory = null, ?StreamFactoryInterface $streamFactory = null): ResponseInterface
    {
        $responseFactory ??= static::getDefaultPsr17Factory();
        $body = $responseEntity->body;
        if ($body) {
            $bodyStream = static::createStreamFromBuffer($body, $streamFactory);
        } else {
            $bodyStream = null;
        }
        if ($responseFactory instanceof Psr17Factory) {
            $response = $responseFactory->createResponse(
                $responseEntity->statusCode,
                $responseEntity->reasonPhrase
            );
            $response
                ->setProtocolVersion($responseEntity->protocolVersion)
                ->setHeadersAndHeaderNames(
                    $responseEntity->headers,
                    $responseEntity->headerNames
                );
            if ($bodyStream) {
                $response->setBody($bodyStream);
            }
            if (!$responseEntity->shouldKeepAlive) {
                $response->setShouldKeepAlive(false);
            }
        } else {
            $response = $responseFactory->createResponse(
                $responseEntity->statusCode,
                $responseEntity->reasonPhrase
            );
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
        $streamFactory ??= static::getDefaultPsr17Factory();
        $uploadedFileFactory ??= static::getDefaultPsr17Factory();
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
        $serverRequestFactory ??= static::getDefaultPsr17Factory();
        $serverRequest = $serverRequestFactory->createServerRequest($method, $uri, $serverParams);
        if ($headers) {
            static::setHeaders($serverRequest, $headers);
        }
        if ($body) {
            static::setBody($serverRequest, $body);
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
        $serverRequestFactory ??= static::getDefaultPsr17Factory();
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
        if ($serverRequest instanceof ServerRequest) {
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

    /** @param array<string, array<string>|string> $headers */
    public static function setHeaders(MessageInterface &$message, array $headers): void
    {
        if ($message instanceof MessagePlusInterface) {
            $message->setHeaders($headers);
        } else {
            static::withHeaders($message, $headers);
        }
    }

    /** @param array<string, array<string>|string> $headers */
    public static function withHeaders(MessageInterface &$message, array $headers): void
    {
        if ($message instanceof MessagePlusInterface) {
            $message = $message->withHeaders($headers);
        } else {
            foreach ($headers as $headerName => $headerValue) {
                $message = $message->withHeader($headerName, $headerValue);
            }
        }
    }

    public static function setBody(MessageInterface &$message, mixed $body): void
    {
        if ($message instanceof MessagePlusInterface) {
            $message->setBody($body);
        } else {
            $message = $message->withBody($body);
        }
    }

    /**
     * @return array<string>
     * @phan-return array{0: string, 1: string}
     * @phpstan-return array{0: string, 1: string}
     * @psalm-return array{0: string, 1: string}
     */
    public static function convertResponseToVector(ResponseInterface $response): array
    {
        if ($response instanceof ResponsePlusInterface) {
            return [
                $response->toString(true),
                (string) $response->getBody(),
            ];
        }
        return [
            Http::packResponse(
                $response->getStatusCode(),
                $response->getHeaders(),
                '',
                $response->getReasonPhrase(),
                $response->getProtocolVersion()
            ),
            (string) $response->getBody(),
        ];
    }
}
