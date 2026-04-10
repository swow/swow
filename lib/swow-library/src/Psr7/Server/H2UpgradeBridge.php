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

use Psr\Http\Message\ServerRequestInterface;
use Swow\Http\Protocol\H2Exception;
use Swow\Http\Status as HttpStatus;
use Swow\Http\Protocol\ProtocolException as HttpProtocolException;

use function base64_decode;
use function str_repeat;
use function strlen;
use function strtr;
use function trim;

final class H2UpgradeBridge
{
    public static function decodeSettingsPayload(ServerRequestInterface $request): string
    {
        $encoded = trim($request->getHeaderLine('HTTP2-Settings'));
        if ($encoded === '') {
            throw new HttpProtocolException(HttpStatus::BAD_REQUEST, 'Missing HTTP2-Settings header for h2c upgrade');
        }

        $normalized = strtr($encoded, '-_', '+/');
        $normalized .= str_repeat('=', (4 - (strlen($normalized) % 4)) % 4);
        $payload = base64_decode($normalized, true);
        if (!is_string($payload) || (strlen($payload) % 6) !== 0) {
            throw new HttpProtocolException(HttpStatus::BAD_REQUEST, 'Invalid HTTP2-Settings header payload');
        }

        return $payload;
    }

    /**
     * @param array{timeout?: ?int, settings?: array<int, int>, limit?: ?int}|array{} $options
     */
    public static function serveUpgradeRequest(
        H2ServerConnection $connection,
        ServerRequestInterface $request,
        callable $handler,
        array $options = []
    ): int {
        $timeout = $options['timeout'] ?? null;
        $limit = $options['limit'] ?? null;
        $streamId = 1;

        $connection->getConnection()->beginUpgradedRequestStream($streamId, true);
        try {
            $response = H2ResponseNormalizer::normalize($handler($request, $streamId));
        } catch (H2Exception $exception) {
            if (!$exception->isConnectionLevel()) {
                $targetStreamId = $exception->getStreamId() > 0 ? $exception->getStreamId() : $streamId;
                $connection->getConnection()->sendRstStream($targetStreamId, $exception->getH2ErrorCode(), $timeout);
            }

            throw $exception;
        }

        $connection->sendTrailedResponse($streamId, $response->response, $response->trailersMap(), $timeout);
        if ($limit === 1) {
            return 1;
        }

        $remaining = $limit === null ? null : max(0, $limit - 1);

        return 1 + $connection->handleRequests($handler, $timeout, $remaining);
    }
}
