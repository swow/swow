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

use RuntimeException;

use const DIRECTORY_SEPARATOR;
use const SORT_STRING;

final class Scanner
{
    /** @var array<string, string> */
    private array $hardlinkMap = [];

    /** @var list<Metadata> */
    private array $entries = [];

    /**
     * @return list<Metadata>
     */
    public static function scan(string $root): array
    {
        $scanner = new self();
        $scanner->walk(rtrim($root, DIRECTORY_SEPARATOR), '');

        return $scanner->entries;
    }

    private function walk(string $dir, string $relative): void
    {
        $names = scandir($dir);
        if ($names === false) {
            throw new RuntimeException("Unable to scan directory: {$dir}");
        }
        sort($names, SORT_STRING);

        foreach ($names as $name) {
            if ($name === '.' || $name === '..') {
                continue;
            }

            $path = $dir . DIRECTORY_SEPARATOR . $name;
            $name = str_replace(DIRECTORY_SEPARATOR, '/', $relative === '' ? $name : $relative . '/' . $name);
            $stat = lstat($path);
            if ($stat === false) {
                throw new RuntimeException("Unable to stat: {$path}");
            }

            $metadata = $this->metadata($name, $stat);
            $this->entries[] = $metadata;

            if (is_link($path)) {
                $target = readlink($path);
                if ($target === false) {
                    throw new RuntimeException("Unable to read link: {$path}");
                }
                $metadata->linkTarget = $target;
                continue;
            }

            if (($stat['mode'] & Metadata::S_IFMT) === Metadata::S_IFDIR) {
                $this->walk($path, $name);
                continue;
            }

            if (($stat['mode'] & Metadata::S_IFMT) !== Metadata::S_IFREG) {
                throw new RuntimeException("Unsupported file type: {$path}");
            }

            if ($stat['nlink'] > 1) {
                $metadata->attr |= Metadata::ATTR_HARDLINK;
                $key = $stat['dev'] . ':' . $stat['ino'];
                if (isset($this->hardlinkMap[$key])) {
                    $metadata->linkTarget = $this->hardlinkMap[$key];
                } else {
                    $this->hardlinkMap[$key] = $name;
                    $metadata->dataTask = $stat['size'] > 0;
                }
            } else {
                $metadata->dataTask = $stat['size'] > 0;
            }
        }
    }

    private function metadata(string $name, array $stat): Metadata
    {
        $metadata = new Metadata();
        $metadata->name = $name;
        $metadata->mode = $stat['mode'] & 0xFFFF;
        $metadata->size = $stat['size'];
        $metadata->uid = $stat['uid'];
        $metadata->gid = $stat['gid'];
        $metadata->mtimeSec = $stat['mtime'];
        $metadata->mtimeNsec = 0;
        $metadata->dev = $stat['dev'];
        $metadata->ino = $stat['ino'];
        $metadata->nlink = $stat['nlink'];

        return $metadata;
    }
}
