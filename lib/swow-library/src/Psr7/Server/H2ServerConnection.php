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

namespace Swow\Psr7\Server;

use Psr\Http\Message\ResponseInterface;
use Psr\Http\Message\ServerRequestInterface;
use Swow\Http\Protocol\H2Connection;
use Swow\Http\Protocol\H2Exception;
use Swow\Http\Protocol\H2Frame;
use Swow\Http\Protocol\H2Hpack;
use Swow\Http\Protocol\H2Headers;
use Swow\Http\Protocol\H2HeaderBlock;
use Swow\Http\Message\ServerRequestEntity;
use Swow\Psr7\Psr7;
use Swow\Socket;

use function substr;
use function strlen;

final class H2ServerConnection extends Socket
{
    use ServerParamsTrait;

    public const REQUEST_TRAILERS_ATTRIBUTE = 'trailers';

    private H2Hpack|null $hpack = null;

    private Socket $managedSocket;

    public function __construct(
        int $type = Socket::TYPE_TCP,
        private H2Connection|null $connection = null,
    ) {
        parent::__construct($type);
        $this->connection ??= new H2Connection($this);
        $this->managedSocket = $this->connection->getSocket();
    }

    /**
     * @param array<string, mixed> $serverParams
     */
    public static function wrap(Socket $socket, array $serverParams = []): static
    {
        $connection = new static(connection: new H2Connection($socket));
        $connection->setServerParams($serverParams);

        return $connection;
    }

    public function getConnection(): H2Connection
    {
        return $this->connection ??= new H2Connection($this);
    }

    public function getSocket(): Socket
    {
        return $this->managedSocket;
    }

    public function readClientPreface(?int $timeout = null): void
    {
        $this->getConnection()->readClientPreface($timeout);
    }

    public function initialize(?int $timeout = null, array $settings = []): void
    {
        $this->getConnection()->initialize($timeout, $settings);
    }

    /**
     * @param array<int, array{name: string, value: string}|array{0: string, 1: string}> $headers
     */
    public function recvRequestEntityFromHeaders(array $headers): ServerRequestEntity
    {
        return H2Headers::createServerRequestEntity($headers, $this->serverParams);
    }

    /**
     * @param array<int, array{name: string, value: string}|array{0: string, 1: string}> $headers
     */
    public function recvRequestFromHeaders(array $headers): ServerRequestInterface
    {
        return Psr7::createServerRequestFromEntity($this->recvRequestEntityFromHeaders($headers));
    }

    public function recvRequestFromHeaderBlock(string $headerBlock): ServerRequestInterface
    {
        return $this->recvRequestFromHeaders($this->getHpack()->decode($headerBlock));
    }

    /**
     * @return array{streamId: int, request: ServerRequestInterface}
     */
    public function recvStreamRequest(?int $timeout = null): array
    {
        $streamRequest = $this->tryRecvStreamRequest($timeout);
        if ($streamRequest !== null) {
            return $streamRequest;
        }

        throw H2Exception::forConnectionError('No more request streams are allowed on this connection');
    }

    /**
     * @return array{streamId: int, request: ServerRequestInterface}|null
     */
    public function tryRecvStreamRequest(?int $timeout = null): ?array
    {
        $connection = $this->getConnection();

        try {
            $headerBlock = $connection->readHeaderBlockOrNull($timeout);
            if ($headerBlock === null) {
                return null;
            }

            $request = $this->recvRequestFromHeaderBlock($headerBlock->getBuffer());
            $streamId = $headerBlock->getStreamId();
            if (($headerBlock->getFlags() & H2Frame::FLAG_END_STREAM) === 0) {
                $bodyWithTrailers = $connection->readRequestBodyWithTrailers($streamId, $timeout);
                $request->getBody()->write($bodyWithTrailers['body']);
                if ($bodyWithTrailers['trailers'] instanceof H2HeaderBlock) {
                    $request = $request->withAttribute(
                        self::REQUEST_TRAILERS_ATTRIBUTE,
                        H2Trailers::fromArray(
                            H2Headers::createTrailerMap(
                                $this->getHpack()->decode($bodyWithTrailers['trailers']->getBuffer())
                            )
                        )
                    );
                }
            }

            return [
                'streamId' => $streamId,
                'request' => $request,
            ];
        } catch (H2Exception $exception) {
            if (!$exception->isConnectionLevel() && $exception->getStreamId() > 0) {
                $connection->sendRstStream($exception->getStreamId(), $exception->getH2ErrorCode(), $timeout);
            }

            throw $exception;
        }
    }

    public function recvRequest(?int $timeout = null): ServerRequestInterface
    {
        return $this->recvStreamRequest($timeout)['request'];
    }

    /**
     * @param array{timeout?: ?int, settings?: array<int, int>, limit?: ?int}|array{} $options
     */
    public function serve(callable $handler, array $options = []): int
    {
        $timeout = $options['timeout'] ?? null;
        $settings = $options['settings'] ?? [];
        $limit = $options['limit'] ?? null;

        $this->readClientPreface($timeout);
        $this->initialize($timeout, $settings);

        return $this->handleRequests($handler, $timeout, $limit);
    }

    public function handleOneRequest(callable $handler, ?int $timeout = null): int
    {
        $streamRequest = $this->recvStreamRequest($timeout);
        $streamId = $streamRequest['streamId'];
        try {
            $response = H2ResponseNormalizer::normalize($handler($streamRequest['request'], $streamId));
        } catch (H2Exception $exception) {
            if (!$exception->isConnectionLevel()) {
                $this->getConnection()->sendRstStream($exception->getStreamId() > 0 ? $exception->getStreamId() : $streamId, $exception->getH2ErrorCode(), $timeout);
            }

            throw $exception;
        }
        $this->sendResponseEnvelope($streamId, $response, $timeout);

        return $streamId;
    }

    public function handleRequests(callable $handler, ?int $timeout = null, ?int $limit = null): int
    {
        $handled = 0;
        while ($limit === null || $handled < $limit) {
            try {
                $streamRequest = $this->tryRecvStreamRequest($timeout);
                if ($streamRequest === null) {
                    break;
                }

                $streamId = $streamRequest['streamId'];
                $response = H2ResponseNormalizer::normalize($handler($streamRequest['request'], $streamId));
                $this->sendResponseEnvelope($streamId, $response, $timeout);
                $handled++;
            } catch (H2Exception $exception) {
                if (!$exception->isConnectionLevel() && isset($streamRequest)) {
                    $targetStreamId = $exception->getStreamId() > 0 ? $exception->getStreamId() : $streamRequest['streamId'];
                    if ($targetStreamId > 0) {
                        $this->getConnection()->sendRstStream($targetStreamId, $exception->getH2ErrorCode(), $timeout);
                    }
                }

                if ($exception->isConnectionLevel()) {
                    throw $exception;
                }
            }
        }

        return $handled;
    }

    public function sendResponse(int $streamId, ResponseInterface $response, ?int $timeout = null): void
    {
        $this->sendResponseEnvelope($streamId, H2ResponseEnvelope::from($response), $timeout);
    }

    /**
     * @param array<string, array<string>|string> $trailers
     */
    public function sendTrailedResponse(
        int $streamId,
        ResponseInterface $response,
        array $trailers,
        ?int $timeout = null
    ): void
    {
        $this->sendResponseEnvelope($streamId, H2ResponseEnvelope::from($response, $trailers), $timeout);
    }

    private function sendResponseEnvelope(int $streamId, H2ResponseEnvelope $responseEnvelope, ?int $timeout = null): void
    {
        $response = $responseEnvelope->response;
        $body = (string) $response->getBody();
        $headers = $response->getHeaders();
        $trailers = $responseEnvelope->trailersMap();
        if ($body === '' && $trailers === []) {
            $this->sendResponseHeaders($streamId, $response->getStatusCode(), $headers, true, $timeout);

            return;
        }

        $this->sendResponseHeaders($streamId, $response->getStatusCode(), $headers, $body === '' && $trailers === [], $timeout);
        if ($body !== '') {
            $this->sendResponseData($streamId, $body, $trailers === [], $timeout);
        }
        if ($trailers !== []) {
            $this->sendResponseTrailers($streamId, $trailers, $timeout);
        }
    }

    /**
     * @return array<string, list<string>>
     */
    public static function getRequestTrailers(ServerRequestInterface $request): array
    {
        return self::requestTrailersObject($request)->toArray();
    }

    public static function requestTrailersObject(ServerRequestInterface $request): H2Trailers
    {
        $trailers = $request->getAttribute(self::REQUEST_TRAILERS_ATTRIBUTE, null);
        if ($trailers instanceof H2Trailers) {
            return $trailers;
        }

        return is_array($trailers) ? H2Trailers::fromArray($trailers) : H2Trailers::empty();
    }

    /**
     * @param array<string, array<string>|string> $headers
     */
    public function sendResponseHeaders(
        int $streamId,
        int $statusCode,
        array $headers = [],
        bool $endStream = false,
        ?int $timeout = null
    ): void {
        $connection = $this->getConnection();
        $connection->assertCanUseResponseStream($streamId);
        $headerBlock = H2Headers::encodeResponseHeaders($statusCode, $headers);
        $headerBlockLength = strlen($headerBlock);
        $maxFrameSize = $connection->getLocalMaxFrameSize();
        $offset = 0;
        $firstChunk = true;
        while ($offset < $headerBlockLength) {
            $chunk = substr($headerBlock, $offset, $maxFrameSize);
            $offset += strlen($chunk);
            $isLast = $offset >= $headerBlockLength;
            $flags = $isLast ? H2Frame::FLAG_END_HEADERS : 0;
            if ($firstChunk && $endStream) {
                $flags |= H2Frame::FLAG_END_STREAM;
            }

            $connection->sendFrame(
                new H2Frame(
                    $firstChunk ? H2Frame::TYPE_HEADERS : H2Frame::TYPE_CONTINUATION,
                    $flags,
                    $streamId,
                    $chunk
                ),
                $timeout
            );
            $firstChunk = false;
        }

        if ($endStream) {
            $connection->noteResponseEnd($streamId);
        }
    }

    public function sendResponseData(
        int $streamId,
        string $data,
        bool $endStream = false,
        ?int $timeout = null
    ): void {
        $connection = $this->getConnection();
        $connection->assertCanUseResponseStream($streamId);
        $maxFrameSize = $connection->getLocalMaxFrameSize();
        $length = strlen($data);
        $offset = 0;
        while ($offset < $length) {
            $availableWindow = $connection->getAvailableSendWindow($streamId);
            if ($availableWindow === 0) {
                $connection->waitForSendWindow($streamId, 1, $timeout);
                $connection->assertCanUseResponseStream($streamId);
                $availableWindow = $connection->getAvailableSendWindow($streamId);
            }

            $chunkSize = min($maxFrameSize, $availableWindow, $length - $offset);
            $chunk = substr($data, $offset, $chunkSize);
            $offset += strlen($chunk);
            $isLast = $offset >= $length;

            $connection->sendFrame(
                new H2Frame(
                    H2Frame::TYPE_DATA,
                    $endStream && $isLast ? H2Frame::FLAG_END_STREAM : 0,
                    $streamId,
                    $chunk
                ),
                $timeout
            );
            $connection->consumeSendWindow($streamId, $chunkSize);
            if ($endStream && $isLast) {
                $connection->noteResponseEnd($streamId);
            }
        }
    }

    /**
     * @param array<string, array<string>|string> $trailers
     */
    public function sendResponseTrailers(int $streamId, array $trailers, ?int $timeout = null): void
    {
        $connection = $this->getConnection();
        $connection->assertCanUseResponseStream($streamId);
        $headerBlock = H2Headers::encodeTrailerHeaders($trailers);
        $headerBlockLength = strlen($headerBlock);
        $maxFrameSize = $connection->getLocalMaxFrameSize();
        $offset = 0;
        $firstChunk = true;
        while ($offset < $headerBlockLength) {
            $chunk = substr($headerBlock, $offset, $maxFrameSize);
            $offset += strlen($chunk);
            $isLast = $offset >= $headerBlockLength;
            $flags = $isLast ? (H2Frame::FLAG_END_HEADERS | H2Frame::FLAG_END_STREAM) : 0;
            $connection->sendFrame(
                new H2Frame(
                    $firstChunk ? H2Frame::TYPE_HEADERS : H2Frame::TYPE_CONTINUATION,
                    $flags,
                    $streamId,
                    $chunk
                ),
                $timeout
            );
            $firstChunk = false;
        }

        $connection->noteResponseEnd($streamId);
    }

    public function goAway(
        int $errorCode = H2Exception::ERROR_NO_ERROR,
        int $lastStreamId = 0,
        string $debugData = '',
        ?int $timeout = null
    ): void {
        $this->getConnection()->goAway($errorCode, $lastStreamId, $debugData, $timeout);
    }

    private function getHpack(): H2Hpack
    {
        return $this->hpack ??= new H2Hpack();
    }

    public function close(): bool
    {
        $socket = $this->getSocket();
        if ($socket === $this) {
            return parent::close();
        }

        return $socket->close();
    }
}
