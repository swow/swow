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
use Swow\Http\Status as HttpStatus;
use Swow\Psr7\Psr7;
use TypeError;

use function get_debug_type;
use function is_array;
use function is_int;
use function array_is_list;
use function sprintf;
use function Swow\Debug\isStrictStringable;

final class H2ResponseNormalizer
{
    public static function normalize(mixed $response): H2ResponseEnvelope
    {
        if ($response instanceof H2ResponseEnvelope) {
            return $response;
        }

        if ($response instanceof ResponseInterface) {
            return H2ResponseEnvelope::from($response);
        }

        if (is_array($response) && !self::isList($response) && self::looksLikeNamedResponseMap($response)) {
            return self::normalizeNamedResponseMap($response);
        }

        $statusCode = HttpStatus::OK;
        $headers = [];
        $body = '';
        $trailers = [];
        $headerArraySeen = false;
        $responseArgs = is_array($response) ? $response : [$response];
        if (is_array($response) && self::isList($response) && $responseArgs !== []) {
            $trailers = self::extractTrailingTrailerArray($responseArgs);
        }
        foreach ($responseArgs as $responseArg) {
            if (isStrictStringable($responseArg)) {
                $body = (string) $responseArg;
                continue;
            }
            if (is_int($responseArg)) {
                $statusCode = $responseArg;
                continue;
            }
            if (is_array($responseArg)) {
                if (!$headerArraySeen) {
                    $headers = $responseArg;
                    $headerArraySeen = true;
                    continue;
                }

                $trailers = $responseArg;
                continue;
            }

            throw new TypeError(sprintf('Unsupported argument type %s', get_debug_type($responseArg)));
        }

        return H2ResponseEnvelope::from(
            Psr7::createResponse(
                code: $statusCode,
                headers: $headers,
                body: $body
            ),
            H2Trailers::fromArray($trailers)
        );
    }

    /**
     * @param list<mixed> $responseArgs
     * @return array<string, array<string>|string>
     */
    private static function extractTrailingTrailerArray(array &$responseArgs): array
    {
        $lastIndex = array_key_last($responseArgs);
        $lastValue = $lastIndex !== null ? $responseArgs[$lastIndex] : null;
        if (!is_array($lastValue) || !self::looksLikeTrailerMap($lastValue)) {
            return [];
        }

        unset($responseArgs[$lastIndex]);
        $responseArgs = array_values($responseArgs);

        return $lastValue;
    }

    /**
     * @param array<mixed> $headers
     */
    private static function looksLikeTrailerMap(array $headers): bool
    {
        foreach ($headers as $name => $_value) {
            if (!is_string($name)) {
                return false;
            }
        }

        return $headers !== [];
    }

    /**
     * @param array<mixed> $response
     */
    private static function looksLikeNamedResponseMap(array $response): bool
    {
        foreach ($response as $key => $_value) {
            if (!is_string($key)) {
                return false;
            }

            if (!in_array($key, ['status', 'headers', 'body', 'trailers'], true)) {
                return false;
            }
        }

        return true;
    }

    /**
     * @param array{status?: mixed, headers?: mixed, body?: mixed, trailers?: mixed} $response
     */
    private static function normalizeNamedResponseMap(array $response): H2ResponseEnvelope
    {
        $status = $response['status'] ?? HttpStatus::OK;
        if (!is_int($status)) {
            throw new TypeError(sprintf('Unsupported argument type %s', get_debug_type($status)));
        }

        $headers = $response['headers'] ?? [];
        if (!is_array($headers)) {
            throw new TypeError(sprintf('Unsupported argument type %s', get_debug_type($headers)));
        }

        $body = $response['body'] ?? '';
        if (!isStrictStringable($body)) {
            throw new TypeError(sprintf('Unsupported argument type %s', get_debug_type($body)));
        }

        $trailers = $response['trailers'] ?? [];
        if (!is_array($trailers)) {
            throw new TypeError(sprintf('Unsupported argument type %s', get_debug_type($trailers)));
        }

        return H2ResponseEnvelope::from(
            Psr7::createResponse(
                code: $status,
                headers: $headers,
                body: (string) $body
            ),
            H2Trailers::fromArray($trailers)
        );
    }

    /**
     * @param array<mixed> $value
     */
    private static function isList(array $value): bool
    {
        $expectedIndex = 0;
        foreach ($value as $key => $_item) {
            if ($key !== $expectedIndex) {
                return false;
            }

            $expectedIndex++;
        }

        return true;
    }
}
