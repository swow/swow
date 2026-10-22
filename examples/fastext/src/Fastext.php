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

use FilesystemIterator;
use RecursiveDirectoryIterator;
use RecursiveIteratorIterator;
use RuntimeException;
use Swow\Examples\Fastext\Compression\CompressorFactory;

use function strlen;

use const JSON_THROW_ON_ERROR;
use const JSON_UNESCAPED_SLASHES;

final class Fastext
{
    private const CHUNK_SIZE = 1024 * 1024;

    public static function archive(
        string $source,
        string $blob,
        int $workers,
        string $mode,
        string $compression,
    ): void {
        $source = realpath($source);
        if ($source === false || !is_dir($source)) {
            throw new RuntimeException('Source directory does not exist');
        }

        $compressor = CompressorFactory::create($compression);
        $entries = Scanner::scan($source);
        $dataTasks = array_values(array_filter(
            $entries,
            static fn(Metadata $metadata): bool => $metadata->dataTask,
        ));

        $runDir = self::createRunDir();
        try {
            $buckets = self::partition($dataTasks, $workers);
            for ($id = 0; $id < $workers; $id++) {
                $files = [];
                foreach ($buckets[$id] as $metadata) {
                    $files[] = ['name' => $metadata->name, 'size' => $metadata->size];
                }
                self::writeJson($runDir . '/archive-' . $id . '.json', [
                    'source' => $source,
                    'part' => $runDir . '/archive-' . $id . '.part',
                    'compression' => $compressor->name(),
                    'files' => $files,
                ]);
            }

            Runner::run($mode, $workers, $runDir, 'archive');

            $byName = [];
            foreach ($entries as $metadata) {
                $byName[$metadata->name] = $metadata;
            }

            $out = fopen($blob, 'w+b');
            if ($out === false) {
                throw new RuntimeException("Unable to open blob: {$blob}");
            }

            try {
                fwrite($out, str_repeat("\0", 8));
                for ($id = 0; $id < $workers; $id++) {
                    $result = json_decode(
                        (string) file_get_contents($runDir . '/archive-' . $id . '.result.json'),
                        true,
                        512,
                        JSON_THROW_ON_ERROR,
                    );
                    $part = fopen($runDir . '/archive-' . $id . '.part', 'rb');
                    if ($part === false) {
                        throw new RuntimeException("Unable to open part for worker {$id}");
                    }
                    try {
                        foreach ($result['files'] as $file) {
                            $metadata = $byName[$file['name']];
                            $metadata->attr = ($metadata->attr & Metadata::ATTR_HARDLINK) | $file['attr'];
                            $metadata->dataOffset = $file['data_size'] > 0 ? ftell($out) : 0;
                            $metadata->dataSize = $file['data_size'];
                            if ($file['data_size'] > 0) {
                                fseek($part, $file['part_offset']);
                                self::copyRange($part, $out, $file['data_size']);
                            }
                        }
                    } finally {
                        fclose($part);
                    }
                }

                $groups = self::buildGroups($entries);
                $metadataOffset = ftell($out);
                $metadataBytes = Metadata::encodeGroups($groups);
                fwrite($out, $metadataBytes);
                ftruncate($out, $metadataOffset + strlen($metadataBytes));
                fseek($out, 0);
                fwrite($out, pack('J', $metadataOffset));
            } finally {
                fclose($out);
            }
        } finally {
            self::removeDir($runDir);
        }
    }

    public static function extract(
        string $blob,
        string $dest,
        int $workers,
        string $mode,
    ): void {
        $entries = self::readMetadata($blob);
        TimestampRestorer::initialize();
        if (!is_dir($dest) && !mkdir($dest, 0755, true) && !is_dir($dest)) {
            throw new RuntimeException("Unable to create destination: {$dest}");
        }

        $dirs = [];
        $symlinks = [];
        $regular = [];
        $hardlinks = [];
        $emptyFiles = [];
        foreach ($entries as $metadata) {
            if ($metadata->isDir()) {
                $dirs[] = $metadata;
            } elseif ($metadata->isSymlink()) {
                $symlinks[] = $metadata;
            } elseif ($metadata->isRegular()) {
                if (($metadata->attr & Metadata::ATTR_HARDLINK) !== 0 && $metadata->linkTarget !== '') {
                    $hardlinks[] = $metadata;
                } elseif ($metadata->dataSize > 0) {
                    $regular[] = $metadata;
                } else {
                    $emptyFiles[] = $metadata;
                }
            }
        }

        usort($dirs, static fn(Metadata $a, Metadata $b): int => substr_count($a->name, '/') <=> substr_count($b->name, '/'));
        foreach ($dirs as $metadata) {
            $path = $dest . '/' . $metadata->name;
            if (!is_dir($path) && !mkdir($path, 0755, true) && !is_dir($path)) {
                throw new RuntimeException("Unable to create directory: {$path}");
            }
        }

        foreach ($symlinks as $metadata) {
            $path = $dest . '/' . $metadata->name;
            if (!symlink($metadata->linkTarget, $path)) {
                throw new RuntimeException("Unable to create symlink: {$path}");
            }
        }

        foreach ($emptyFiles as $metadata) {
            $path = $dest . '/' . $metadata->name;
            if (file_put_contents($path, '') === false) {
                throw new RuntimeException("Unable to create file: {$path}");
            }
        }

        $runDir = self::createRunDir();
        try {
            $buckets = self::partition($regular, $workers);
            for ($id = 0; $id < $workers; $id++) {
                $files = [];
                foreach ($buckets[$id] as $metadata) {
                    $files[] = [
                        'name' => $metadata->name,
                        'data_offset' => $metadata->dataOffset,
                        'data_size' => $metadata->dataSize,
                        'size' => $metadata->size,
                        'attr' => $metadata->attr,
                        'mode' => $metadata->mode,
                    ];
                }
                self::writeJson($runDir . '/extract-' . $id . '.json', [
                    'blob' => realpath($blob),
                    'dest' => realpath($dest),
                    'files' => $files,
                ]);
            }

            Runner::run($mode, $workers, $runDir, 'extract');

            foreach ($hardlinks as $metadata) {
                $target = $dest . '/' . $metadata->linkTarget;
                $path = $dest . '/' . $metadata->name;
                if (!link($target, $path)) {
                    throw new RuntimeException("Unable to create hardlink: {$path}");
                }
            }

            foreach ($entries as $metadata) {
                if ($metadata->isSymlink()) {
                    continue;
                }
                $path = $dest . '/' . $metadata->name;
                chmod($path, $metadata->mode & 0777);
                TimestampRestorer::restore($path, $metadata->mtimeSec);
            }
            foreach (array_reverse($dirs) as $metadata) {
                TimestampRestorer::restore($dest . '/' . $metadata->name, $metadata->mtimeSec);
            }
        } finally {
            self::removeDir($runDir);
        }
    }

    /**
     * @return list<Metadata>
     */
    public static function readMetadata(string $blob): array
    {
        $in = fopen($blob, 'rb');
        if ($in === false) {
            throw new RuntimeException("Unable to open blob: {$blob}");
        }
        try {
            $header = fread($in, 8);
            if (strlen($header) !== 8) {
                throw new RuntimeException('Invalid blob header');
            }
            $offset = unpack('Joffset', $header)['offset'];
            if ($offset < 8 || fseek($in, $offset) !== 0) {
                throw new RuntimeException('Invalid metadata offset');
            }
            $data = stream_get_contents($in);
            if ($data === false) {
                throw new RuntimeException('Unable to read metadata');
            }
        } finally {
            fclose($in);
        }

        $entries = [];
        foreach (Metadata::decodeGroups($data) as $group) {
            foreach ($group as $metadata) {
                $entries[] = $metadata;
            }
        }

        return $entries;
    }

    /**
     * @param list<Metadata> $entries
     * @return list<list<Metadata>>
     */
    private static function buildGroups(array $entries): array
    {
        $dirs = [];
        $hardlinks = [];
        $rest = [];
        foreach ($entries as $metadata) {
            if ($metadata->isDir()) {
                $depth = substr_count($metadata->name, '/');
                $dirs[$depth][] = $metadata;
            } elseif ($metadata->isRegular() &&
                ($metadata->attr & Metadata::ATTR_HARDLINK) !== 0 &&
                $metadata->dataSize > 0) {
                $hardlinks[] = $metadata;
            } else {
                $rest[] = $metadata;
            }
        }
        ksort($dirs);
        $groups = array_values($dirs);
        if ($hardlinks !== []) {
            $groups[] = $hardlinks;
        }
        if ($rest !== []) {
            $groups[] = $rest;
        }

        return $groups;
    }

    /**
     * @param list<Metadata> $tasks
     * @return list<list<Metadata>>
     */
    private static function partition(array $tasks, int $workers): array
    {
        usort($tasks, static fn(Metadata $a, Metadata $b): int => $b->size <=> $a->size);
        $buckets = array_fill(0, $workers, []);
        $sizes = array_fill(0, $workers, 0);
        foreach ($tasks as $metadata) {
            $index = 0;
            for ($i = 1; $i < $workers; $i++) {
                if ($sizes[$i] < $sizes[$index]) {
                    $index = $i;
                }
            }
            $buckets[$index][] = $metadata;
            $sizes[$index] += $metadata->size;
        }

        return $buckets;
    }

    private static function writeJson(string $path, array $data): void
    {
        $json = json_encode($data, JSON_THROW_ON_ERROR | JSON_UNESCAPED_SLASHES);
        if (file_put_contents($path, $json) === false) {
            throw new RuntimeException("Unable to write: {$path}");
        }
    }

    private static function copyRange($in, $out, int $length): void
    {
        $remaining = $length;
        while ($remaining > 0) {
            $chunk = fread($in, min(self::CHUNK_SIZE, $remaining));
            if ($chunk === false || $chunk === '') {
                throw new RuntimeException('Unexpected end of data');
            }
            if (fwrite($out, $chunk) !== strlen($chunk)) {
                throw new RuntimeException('Unable to write data');
            }
            $remaining -= strlen($chunk);
        }
    }

    private static function createRunDir(): string
    {
        $dir = sys_get_temp_dir() . '/fastext-' . getmypid() . '-' . bin2hex(random_bytes(6));
        if (!mkdir($dir, 0700, true) && !is_dir($dir)) {
            throw new RuntimeException("Unable to create run directory: {$dir}");
        }

        return $dir;
    }

    private static function removeDir(string $dir): void
    {
        if (!is_dir($dir)) {
            return;
        }
        $iterator = new RecursiveIteratorIterator(
            new RecursiveDirectoryIterator($dir, FilesystemIterator::SKIP_DOTS),
            RecursiveIteratorIterator::CHILD_FIRST,
        );
        foreach ($iterator as $item) {
            if ($item->isDir() && !$item->isLink()) {
                @rmdir($item->getPathname());
            } else {
                @unlink($item->getPathname());
            }
        }
        @rmdir($dir);
    }
}
