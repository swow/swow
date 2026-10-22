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

namespace Swow\Examples\Fastext;

final class Metadata
{
    public const ATTR_COMPRESS_ZSTD = 1 << 0;
    public const ATTR_HARDLINK = 1 << 1;
    public const ATTR_COMPRESS_ZLIB = 1 << 2;

    public const S_IFMT = 0170000;
    public const S_IFREG = 0100000;
    public const S_IFDIR = 0040000;
    public const S_IFLNK = 0120000;

    private const HEADER_SIZE = 40;

    public int $attr = 0;
    public int $mode = 0;
    public int $size = 0;
    public int $uid = 0;
    public int $gid = 0;
    public int $mtimeSec = 0;
    public int $mtimeNsec = 0;
    public string $name = '';
    public int $dataOffset = 0;
    public int $dataSize = 0;
    public string $linkTarget = '';

    public bool $dataTask = false;
    public int $dev = 0;
    public int $ino = 0;
    public int $nlink = 0;

    public function isDir(): bool
    {
        return ($this->mode & self::S_IFMT) === self::S_IFDIR;
    }

    public function isRegular(): bool
    {
        return ($this->mode & self::S_IFMT) === self::S_IFREG;
    }

    public function isSymlink(): bool
    {
        return ($this->mode & self::S_IFMT) === self::S_IFLNK;
    }

    public function encode(): string
    {
        $header = pack(
            'nnJNNJJN',
            $this->attr,
            $this->mode,
            $this->size,
            $this->uid,
            $this->gid,
            $this->mtimeSec,
            $this->mtimeNsec,
            strlen($this->name),
        );

        if ($this->isRegular() && ($this->attr & self::ATTR_HARDLINK) !== 0) {
            return $header
                . pack('NJJ', strlen($this->linkTarget), $this->dataOffset, $this->dataSize)
                . $this->name
                . $this->linkTarget;
        }
        if ($this->isRegular()) {
            return $header
                . pack('JJ', $this->dataOffset, $this->dataSize)
                . $this->name;
        }
        if ($this->isDir()) {
            return $header . $this->name;
        }
        if ($this->isSymlink()) {
            return $header
                . pack('N', strlen($this->linkTarget))
                . $this->name
                . $this->linkTarget;
        }

        throw new \RuntimeException(sprintf('Unsupported file type: %o', $this->mode));
    }

    public static function decode(string $data, int $offset = 0): self
    {
        $length = strlen($data);
        if ($offset + self::HEADER_SIZE > $length) {
            throw new \RuntimeException('Truncated metadata header');
        }

        $header = unpack(
            'nattr/nmode/Jsize/Nuid/Ngid/Jmtime_sec/Jmtime_nsec/Nname_len',
            substr($data, $offset, self::HEADER_SIZE),
        );
        if ($header === false) {
            throw new \RuntimeException('Invalid metadata header');
        }

        $metadata = new self();
        $metadata->attr = $header['attr'];
        $metadata->mode = $header['mode'];
        $metadata->size = $header['size'];
        $metadata->uid = $header['uid'];
        $metadata->gid = $header['gid'];
        $metadata->mtimeSec = $header['mtime_sec'];
        $metadata->mtimeNsec = $header['mtime_nsec'];
        $nameLength = $header['name_len'];

        $offset += self::HEADER_SIZE;
        if ($metadata->isRegular() && ($metadata->attr & self::ATTR_HARDLINK) !== 0) {
            self::requireBytes($data, $offset, 20);
            $body = unpack('Nlink_len/Jdata_offset/Jdata_size', substr($data, $offset, 20));
            if ($body === false) {
                throw new \RuntimeException('Invalid hardlink metadata');
            }
            $offset += 20;
            $linkLength = $body['link_len'];
            $metadata->dataOffset = $body['data_offset'];
            $metadata->dataSize = $body['data_size'];
            self::requireBytes($data, $offset, $nameLength + $linkLength);
            $metadata->name = substr($data, $offset, $nameLength);
            $offset += $nameLength;
            $metadata->linkTarget = substr($data, $offset, $linkLength);
            $offset += $linkLength;
        } elseif ($metadata->isRegular()) {
            self::requireBytes($data, $offset, 16);
            $body = unpack('Jdata_offset/Jdata_size', substr($data, $offset, 16));
            if ($body === false) {
                throw new \RuntimeException('Invalid regular metadata');
            }
            $offset += 16;
            $metadata->dataOffset = $body['data_offset'];
            $metadata->dataSize = $body['data_size'];
            self::requireBytes($data, $offset, $nameLength);
            $metadata->name = substr($data, $offset, $nameLength);
            $offset += $nameLength;
        } elseif ($metadata->isDir()) {
            $nameStart = $offset;
            self::requireBytes($data, $offset, $nameLength);
            $metadata->name = substr($data, $offset, $nameLength);
            $offset += $nameLength;
        } elseif ($metadata->isSymlink()) {
            self::requireBytes($data, $offset, 4);
            $body = unpack('Nlink_len', substr($data, $offset, 4));
            if ($body === false) {
                throw new \RuntimeException('Invalid symlink metadata');
            }
            $offset += 4;
            $linkLength = $body['link_len'];
            self::requireBytes($data, $offset, $nameLength + $linkLength);
            $metadata->name = substr($data, $offset, $nameLength);
            $offset += $nameLength;
            $metadata->linkTarget = substr($data, $offset, $linkLength);
            $offset += $linkLength;
        } else {
            throw new \RuntimeException(sprintf('Unsupported file type: %o', $metadata->mode));
        }

        return $metadata;
    }

    public static function encodeList(array $list): string
    {
        $data = '';
        foreach ($list as $metadata) {
            $data .= $metadata->encode();
        }

        return $data;
    }

    public static function decodeList(string $data): array
    {
        $list = [];
        $offset = 0;
        $length = strlen($data);
        while ($offset < $length) {
            $metadata = self::decode($data, $offset);
            $list[] = $metadata;
            $offset = self::recordEnd($data, $offset);
        }

        return $list;
    }

    public static function encodeGroups(array $groups): string
    {
        $data = '';
        foreach ($groups as $group) {
            $list = self::encodeList($group);
            $data .= pack('N', strlen($list)) . $list;
        }

        return $data;
    }

    public static function decodeGroups(string $data): array
    {
        $groups = [];
        $offset = 0;
        $length = strlen($data);
        while ($offset < $length) {
            self::requireBytes($data, $offset, 4);
            $groupLength = unpack('Nlength', substr($data, $offset, 4))['length'];
            $offset += 4;
            self::requireBytes($data, $offset, $groupLength);
            $groups[] = self::decodeList(substr($data, $offset, $groupLength));
            $offset += $groupLength;
        }

        return $groups;
    }

    private static function requireBytes(string $data, int $offset, int $length): void
    {
        if ($length < 0 || $offset + $length > strlen($data)) {
            throw new \RuntimeException('Truncated metadata record');
        }
    }

    private static function recordEnd(string $data, int $offset): int
    {
        $length = strlen($data);
        if ($offset + self::HEADER_SIZE > $length) {
            throw new \RuntimeException('Truncated metadata record');
        }
        $header = unpack(
            'nattr/nmode/Jsize/Nuid/Ngid/Jmtime_sec/Jmtime_nsec/Nname_len',
            substr($data, $offset, self::HEADER_SIZE),
        );
        $attr = $header['attr'];
        $mode = $header['mode'];
        $nameLength = $header['name_len'];
        $offset += self::HEADER_SIZE;
        $type = $mode & self::S_IFMT;

        if ($type === self::S_IFREG && ($attr & self::ATTR_HARDLINK) !== 0) {
            self::requireBytes($data, $offset, 20);
            $body = unpack('Nlink_len/Jdata_offset/Jdata_size', substr($data, $offset, 20));
            return $offset + 20 + $nameLength + $body['link_len'];
        }
        if ($type === self::S_IFREG) {
            return $offset + 16 + $nameLength;
        }
        if ($type === self::S_IFDIR) {
            return $offset + $nameLength;
        }
        if ($type === self::S_IFLNK) {
            self::requireBytes($data, $offset, 4);
            $body = unpack('Nlink_len', substr($data, $offset, 4));
            return $offset + 4 + $nameLength + $body['link_len'];
        }

        throw new \RuntimeException(sprintf('Unsupported file type: %o', $mode));
    }
}
