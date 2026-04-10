<?php

declare(strict_types=1);

namespace Swow\Http\Protocol;

use function ord;
use function strlen;
use function substr;

final class H2Hpack
{
    /**
     * @var list<array{name: string, value: string}>
     */
    private const STATIC_TABLE = [
        ['name' => ':authority', 'value' => ''],
        ['name' => ':method', 'value' => 'GET'],
        ['name' => ':method', 'value' => 'POST'],
        ['name' => ':path', 'value' => '/'],
        ['name' => ':path', 'value' => '/index.html'],
        ['name' => ':scheme', 'value' => 'http'],
        ['name' => ':scheme', 'value' => 'https'],
        ['name' => ':status', 'value' => '200'],
        ['name' => ':status', 'value' => '204'],
        ['name' => ':status', 'value' => '206'],
        ['name' => ':status', 'value' => '304'],
        ['name' => ':status', 'value' => '400'],
        ['name' => ':status', 'value' => '404'],
        ['name' => ':status', 'value' => '500'],
        ['name' => 'accept-charset', 'value' => ''],
        ['name' => 'accept-encoding', 'value' => 'gzip, deflate'],
        ['name' => 'accept-language', 'value' => ''],
        ['name' => 'accept-ranges', 'value' => ''],
        ['name' => 'accept', 'value' => ''],
        ['name' => 'access-control-allow-origin', 'value' => ''],
        ['name' => 'age', 'value' => ''],
        ['name' => 'allow', 'value' => ''],
        ['name' => 'authorization', 'value' => ''],
        ['name' => 'cache-control', 'value' => ''],
        ['name' => 'content-disposition', 'value' => ''],
        ['name' => 'content-encoding', 'value' => ''],
        ['name' => 'content-language', 'value' => ''],
        ['name' => 'content-length', 'value' => ''],
        ['name' => 'content-location', 'value' => ''],
        ['name' => 'content-range', 'value' => ''],
        ['name' => 'content-type', 'value' => ''],
        ['name' => 'cookie', 'value' => ''],
        ['name' => 'date', 'value' => ''],
        ['name' => 'etag', 'value' => ''],
        ['name' => 'expect', 'value' => ''],
        ['name' => 'expires', 'value' => ''],
        ['name' => 'from', 'value' => ''],
        ['name' => 'host', 'value' => ''],
        ['name' => 'if-match', 'value' => ''],
        ['name' => 'if-modified-since', 'value' => ''],
        ['name' => 'if-none-match', 'value' => ''],
        ['name' => 'if-range', 'value' => ''],
        ['name' => 'if-unmodified-since', 'value' => ''],
        ['name' => 'last-modified', 'value' => ''],
        ['name' => 'link', 'value' => ''],
        ['name' => 'location', 'value' => ''],
        ['name' => 'max-forwards', 'value' => ''],
        ['name' => 'proxy-authenticate', 'value' => ''],
        ['name' => 'proxy-authorization', 'value' => ''],
        ['name' => 'range', 'value' => ''],
        ['name' => 'referer', 'value' => ''],
        ['name' => 'refresh', 'value' => ''],
        ['name' => 'retry-after', 'value' => ''],
        ['name' => 'server', 'value' => ''],
        ['name' => 'set-cookie', 'value' => ''],
        ['name' => 'strict-transport-security', 'value' => ''],
        ['name' => 'transfer-encoding', 'value' => ''],
        ['name' => 'user-agent', 'value' => ''],
        ['name' => 'vary', 'value' => ''],
        ['name' => 'via', 'value' => ''],
        ['name' => 'www-authenticate', 'value' => ''],
    ];

    private H2HpackDynamicTable $dynamicTable;

    public function __construct(
        private int $maxTableSize = 4096,
        private int $maxHeaderListSize = 16384,
    ) {
        $this->dynamicTable = new H2HpackDynamicTable($maxTableSize);
    }

    public function resize(int $maxTableSize): void
    {
        if ($maxTableSize < 0) {
            throw H2Exception::forConnectionError('HPACK table size must not be negative');
        }

        $this->maxTableSize = $maxTableSize;
        $this->dynamicTable->setMaxSize($maxTableSize);
    }

    /**
     * @return list<array{name: string, value: string}>
     */
    public function decode(string $block): array
    {
        $headers = [];
        $offset = 0;
        $length = strlen($block);
        $totalSize = 0;
        $pastFirstHeader = false;
        $tableSizeUpdates = 0;

        while ($offset < $length) {
            $byte = ord($block[$offset]);

            if (($byte & 0b1110_0000) === 0b0010_0000) {
                if ($pastFirstHeader) {
                    throw H2Exception::forConnectionError('HPACK table size update must appear at block start');
                }

                $tableSizeUpdates++;
                if ($tableSizeUpdates > 2) {
                    throw H2Exception::forConnectionError('HPACK too many dynamic table size updates');
                }

                [$newSize, $offset] = H2HpackInteger::decode($block, $offset, 5, $length);
                if ($newSize > $this->maxTableSize) {
                    throw H2Exception::forConnectionError('HPACK table size update exceeds allowed maximum');
                }

                $this->dynamicTable->setMaxSize($newSize);
                continue;
            }

            $pastFirstHeader = true;
            $sensitive = false;

            if (($byte & 0b1000_0000) !== 0) {
                $index = $byte & 0x7F;
                if ($index < 0x7F) {
                    $offset++;
                } else {
                    [$index, $offset] = H2HpackInteger::decode($block, $offset, 7, $length);
                }

                $entry = $this->lookupIndex($index);
                $name = $entry['name'];
                $value = $entry['value'];
            } elseif (($byte & 0b0100_0000) !== 0) {
                $index = $byte & 0x3F;
                if ($index < 0x3F) {
                    $offset++;
                } else {
                    [$index, $offset] = H2HpackInteger::decode($block, $offset, 6, $length);
                }

                if ($index > 0) {
                    $entry = $this->lookupIndex($index);
                    $name = $entry['name'];
                } else {
                    [$name, $offset] = $this->decodeString($block, $offset, $length);
                }

                [$value, $offset] = $this->decodeString($block, $offset, $length);
                $this->dynamicTable->insert($name, $value);
            } elseif (($byte & 0b1111_0000) === 0b0001_0000) {
                $index = $byte & 0x0F;
                if ($index < 0x0F) {
                    $offset++;
                } else {
                    [$index, $offset] = H2HpackInteger::decode($block, $offset, 4, $length);
                }

                if ($index > 0) {
                    $entry = $this->lookupIndex($index);
                    $name = $entry['name'];
                } else {
                    [$name, $offset] = $this->decodeString($block, $offset, $length);
                }

                [$value, $offset] = $this->decodeString($block, $offset, $length);
                $sensitive = true;
            } else {
                $index = $byte & 0x0F;
                if ($index < 0x0F) {
                    $offset++;
                } else {
                    [$index, $offset] = H2HpackInteger::decode($block, $offset, 4, $length);
                }

                if ($index > 0) {
                    $entry = $this->lookupIndex($index);
                    $name = $entry['name'];
                } else {
                    [$name, $offset] = $this->decodeString($block, $offset, $length);
                }

                [$value, $offset] = $this->decodeString($block, $offset, $length);
            }

            $totalSize += strlen($name) + strlen($value) + 32;
            if ($totalSize > $this->maxHeaderListSize) {
                throw H2Exception::forConnectionError('HPACK decoded header list exceeds configured limit');
            }

            $headers[] = ['name' => $name, 'value' => $value];
            if ($sensitive) {
                continue;
            }
        }

        return $headers;
    }

    /**
     * @return array{string, int}
     */
    private function decodeString(string $data, int $offset, int $dataLength): array
    {
        if ($offset >= $dataLength) {
            throw H2Exception::forConnectionError('HPACK string literal is truncated');
        }

        $huffman = (ord($data[$offset]) & 0b1000_0000) !== 0;
        [$stringLength, $offset] = H2HpackInteger::decode($data, $offset, 7, $dataLength);
        if (($offset + $stringLength) > $dataLength) {
            throw H2Exception::forConnectionError('HPACK string literal is truncated');
        }

        $stringData = substr($data, $offset, $stringLength);
        $offset += $stringLength;

        if ($huffman) {
            $stringData = H2HpackHuffman::decode($stringData);
        }

        return [$stringData, $offset];
    }

    /**
     * @return array{string, string}
     */
    private function lookupIndex(int $index): array
    {
        if ($index === 0) {
            throw H2Exception::forConnectionError('HPACK index must be positive');
        }

        if ($index <= 61) {
            return self::STATIC_TABLE[$index - 1];
        }

        $entry = $this->dynamicTable->get($index - 62);
        if ($entry !== null) {
            return $entry;
        }

        throw H2Exception::forConnectionError('HPACK index out of range');
    }
}
