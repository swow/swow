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

use RuntimeException;
use SimpleXMLElement;
use Swow\Http\Mime\MimeType;

use function array_walk_recursive;
use function http_build_query;
use function json_encode;

use const JSON_THROW_ON_ERROR;

class BodyEncoder
{
    /** @param array<mixed> $data */
    public static function encode(array $data, string $contentType = ''): string
    {
        return match ($contentType) {
            MimeType::JSON => static::encodeJson($data),
            MimeType::XML => static::encodeXml($data),
            default => static::encodeQuery($data),
        };
    }

    /** @param array<mixed> $data */
    public static function encodeJson(array $data): string
    {
        return json_encode($data, JSON_THROW_ON_ERROR);
    }

    /** @param array<mixed> $data */
    public static function encodeXml(array $data): string
    {
        $xml = new SimpleXMLElement('<root/>');
        array_walk_recursive($data, [$xml, 'addChild']);
        $xmlString = $xml->asXML();
        if (!$xmlString) {
            throw new RuntimeException('XML encode failed');
        }
        return $xmlString;
    }

    /** @param array<mixed> $data */
    public static function encodeQuery(array $data): string
    {
        return http_build_query($data);
    }
}
