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
use Swow\Http;
use Swow\Http\ResponseEntity;
use Swow\Http\ServerRequestEntity;
use Swow\Http\UploadedFileEntity;

use function is_resource;
use function parse_str;

class PsrHelper
{
    public static function createUriFromString(string $uri, ?UriFactoryInterface $uriFactory = null): UriInterface
    {
        $uriFactory ??= Psr17Factory::getInstance();
        return $uriFactory->createUri($uri);
    }

    public static function createStream(string $data = '', ?StreamFactoryInterface $streamFactory = null): StreamInterface
    {
        return ($streamFactory ?? Psr17Factory::getInstance())->createStream($data);
    }

    public static function createStreamFromBuffer(Buffer $buffer, ?StreamFactoryInterface $streamFactory = null): StreamInterface
    {
        $streamFactory ??= Psr17Factory::getInstance();
        if ($streamFactory instanceof Psr17Factory) {
            return $streamFactory->createStreamFromBuffer($buffer);
        } else {
            return $streamFactory->createStream((string) $buffer);
        }
    }

    public static function createStreamFromAny(mixed $data = '', ?StreamFactoryInterface $streamFactory = null): StreamInterface
    {
        if ($data instanceof StreamInterface) {
            return $data;
        }
        $streamFactory ??= Psr17Factory::getInstance();
        if (is_resource($data)) {
            return $streamFactory->createStreamFromResource($data);
        }
        if ($data instanceof Buffer && $streamFactory instanceof Psr17Factory) {
            return $streamFactory->createStreamFromBuffer($data);
        } else {
            return $streamFactory->createStream((string) $data);
        }
    }

    public static function createResponseFromEntity(ResponseEntity $responseEntity, ?ResponseFactoryInterface $responseFactory = null, ?StreamFactoryInterface $streamFactory = null): ResponseInterface
    {
        $responseFactory ??= Psr17Factory::getInstance();
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
     * @param array<UploadedFileEntity> $uploadedFileEntities
     * @return array<UploadedFileInterface>
     */
    public static function createUploadedFilesFromEntity(array $uploadedFileEntities, ?StreamFactoryInterface $streamFactory = null, ?UploadedFileFactoryInterface $uploadedFileFactory = null): array
    {
        $streamFactory ??= Psr17Factory::getInstance();
        $uploadedFileFactory ??= Psr17Factory::getInstance();
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

    public static function createServerRequestFromEntity(
        ServerRequestEntity $serverRequestEntity,
        ?ServerRequestFactoryInterface $serverRequestFactory = null,
        ?UriFactoryInterface $uriFactory = null,
        ?StreamFactoryInterface $streamFactory = null,
        ?UploadedFileFactoryInterface $uploadedFileFactory = null
    ): ServerRequestInterface {
        $serverRequestFactory ??= Psr17Factory::getInstance();
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

    /**
     * @param array<string, string> $headers
     */
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
