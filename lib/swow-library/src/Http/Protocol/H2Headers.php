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

namespace Swow\Http\Protocol;

use Swow\Http\Message\ServerRequestEntity;

use function array_key_exists;
use function explode;
use function is_array;
use function is_string;
use function preg_match;
use function strlen;
use function strtolower;
use function trim;

final class H2Headers
{
    /**
     * @param array<int, array{name: string, value: string}|array{0: string, 1: string}> $headers
     */
    public static function createServerRequestEntity(array $headers, array $serverParams = []): ServerRequestEntity
    {
        $entity = new ServerRequestEntity();
        $entity->protocolVersion = '2.0';
        $entity->shouldKeepAlive = true;
        $entity->serverParams = $serverParams + $entity->serverParams;

        $pseudoHeaders = [];
        foreach ($headers as $header) {
            [$name, $value] = self::normalizeHeader($header);

            if ($name === '') {
                throw H2Exception::forConnectionError('H2 header name must not be empty');
            }

            if ($name[0] === ':') {
                if (array_key_exists($name, $pseudoHeaders)) {
                    throw H2Exception::forConnectionError('Duplicate pseudo-header "' . $name . '"');
                }

                $pseudoHeaders[$name] = $value;
                continue;
            }

            if (!preg_match('/^[a-z0-9!#$%&\'*+.^_`|~-]+$/', $name)) {
                throw H2Exception::forConnectionError('Invalid H2 header name "' . $name . '"');
            }

            $entity->headers[$name] ??= [];
            $entity->headers[$name][] = $value;
            $entity->headerNames[$name] ??= $name;
        }

        $entity->method = self::requirePseudoHeader($pseudoHeaders, ':method');
        $entity->uri = self::buildRequestUri($pseudoHeaders);

        if (isset($pseudoHeaders[':authority']) && !isset($entity->headers['host'])) {
            $entity->headers['host'] = [$pseudoHeaders[':authority']];
            $entity->headerNames['host'] = 'host';
        }

        if (isset($entity->headers['cookie'])) {
            $entity->cookies = self::parseCookies($entity->headers['cookie']);
        }

        return $entity;
    }

    /**
     * @param array<string, array<string>|string> $headers
     */
    public static function encodeResponseHeaders(int $statusCode, array $headers = []): string
    {
        $status = (string) $statusCode;
        $block = self::encodeLiteralHeader(':status', $status);

        foreach ($headers as $name => $value) {
            $values = is_array($value) ? $value : [$value];
            $lowerName = strtolower($name);

            if ($lowerName === '' || $lowerName[0] === ':') {
                throw H2Exception::forConnectionError('Response headers must not contain pseudo-headers');
            }

            foreach ($values as $singleValue) {
                $block .= self::encodeLiteralHeader($lowerName, (string) $singleValue);
            }
        }

        return $block;
    }

    /**
     * @param array<int, array{name: string, value: string}|array{0: string, 1: string}> $headers
     * @return array<string, list<string>>
     */
    public static function createTrailerMap(array $headers): array
    {
        $trailers = [];
        foreach ($headers as $header) {
            [$name, $value] = self::normalizeHeader($header);

            if ($name === '') {
                throw H2Exception::forConnectionError('H2 trailer name must not be empty');
            }

            if ($name[0] === ':') {
                throw H2Exception::forConnectionError('Trailer headers must not contain pseudo-headers');
            }

            if (!preg_match('/^[a-z0-9!#$%&\'*+.^_`|~-]+$/', $name)) {
                throw H2Exception::forConnectionError('Invalid H2 trailer name "' . $name . '"');
            }

            $trailers[$name] ??= [];
            $trailers[$name][] = $value;
        }

        return $trailers;
    }

    /**
     * @param array<string, array<string>|string> $headers
     */
    public static function encodeTrailerHeaders(array $headers): string
    {
        $block = '';
        foreach ($headers as $name => $value) {
            $values = is_array($value) ? $value : [$value];
            $lowerName = strtolower($name);

            if ($lowerName === '' || $lowerName[0] === ':') {
                throw H2Exception::forConnectionError('Trailer headers must not contain pseudo-headers');
            }

            foreach ($values as $singleValue) {
                $block .= self::encodeLiteralHeader($lowerName, (string) $singleValue);
            }
        }

        return $block;
    }

    /**
     * @param array{name: string, value: string}|array{0: string, 1: string} $header
     * @return array{string, string}
     */
    private static function normalizeHeader(array $header): array
    {
        if (isset($header['name'], $header['value'])) {
            return [strtolower($header['name']), $header['value']];
        }

        if (isset($header[0], $header[1]) && is_string($header[0]) && is_string($header[1])) {
            return [strtolower($header[0]), $header[1]];
        }

        throw H2Exception::forConnectionError('Invalid H2 header tuple');
    }

    /**
     * @param array<string, string> $pseudoHeaders
     */
    private static function buildRequestUri(array $pseudoHeaders): string
    {
        $authority = self::requirePseudoHeader($pseudoHeaders, ':authority');
        $method = self::requirePseudoHeader($pseudoHeaders, ':method');

        if ($method === 'CONNECT') {
            return $authority;
        }

        $scheme = self::requirePseudoHeader($pseudoHeaders, ':scheme');
        $path = self::requirePseudoHeader($pseudoHeaders, ':path');

        return $scheme . '://' . $authority . $path;
    }

    /**
     * @param array<string, string> $pseudoHeaders
     */
    private static function requirePseudoHeader(array $pseudoHeaders, string $name): string
    {
        $value = $pseudoHeaders[$name] ?? '';
        if ($value === '') {
            throw H2Exception::forConnectionError('Missing required pseudo-header "' . $name . '"');
        }

        return $value;
    }

    /**
     * @param array<string> $cookieHeaders
     * @return array<string, string>
     */
    private static function parseCookies(array $cookieHeaders): array
    {
        $cookies = [];
        foreach ($cookieHeaders as $cookieHeader) {
            $pairs = explode(';', $cookieHeader);
            foreach ($pairs as $pair) {
                $pair = trim($pair);
                if ($pair === '') {
                    continue;
                }

                [$name, $value] = array_pad(explode('=', $pair, 2), 2, '');
                $name = trim($name);
                if ($name === '') {
                    continue;
                }

                $cookies[$name] = trim($value);
            }
        }

        return $cookies;
    }

    private static function encodeLiteralHeader(string $name, string $value): string
    {
        return
            "\x00" .
            self::encodeString($name) .
            self::encodeString($value);
    }

    private static function encodeString(string $value): string
    {
        return self::encodeInteger(strlen($value), 7, 0) . $value;
    }

    private static function encodeInteger(int $value, int $prefixBits, int $prefixMask): string
    {
        $maxPrefixValue = (1 << $prefixBits) - 1;
        if ($value < $maxPrefixValue) {
            return chr($prefixMask | $value);
        }

        $encoded = chr($prefixMask | $maxPrefixValue);
        $value -= $maxPrefixValue;

        while ($value >= 128) {
            $encoded .= chr(($value % 128) + 128);
            $value = intdiv($value, 128);
        }

        return $encoded . chr($value);
    }
}
