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

use Swow\Examples\Fastext\Compression\CompressorFactory;

final class Worker
{
    private const CHUNK_SIZE = 1024 * 1024;
    private const COMPRESS_RATIO_THRESHOLD = 0.8;

    public static function runFromEnvironment(string $runDir, string $action): bool
    {
        $id = getenv('SWOW_PROCESS_MANAGER_WORKER_ID');
        if ($id === false || $id === '') {
            throw new \RuntimeException('SWOW_PROCESS_MANAGER_WORKER_ID is missing');
        }

        return self::runFromRunDir($runDir, $action, (int) $id);
    }

    public static function runFromRunDir(string $runDir, string $action, int $id): bool
    {
        $resultPath = $runDir . '/' . $action . '-' . $id . '.result.json';
        try {
            if ($action === 'archive') {
                $result = self::archive($runDir, $id);
            } elseif ($action === 'extract') {
                $result = self::extract($runDir, $id);
            } else {
                throw new \RuntimeException("Unknown action: {$action}");
            }
            self::writeResult($resultPath, ['ok' => true] + $result);

            return true;
        } catch (\Throwable $e) {
            self::writeResult($resultPath, [
                'ok' => false,
                'error' => $e::class . ': ' . $e->getMessage(),
            ]);

            return false;
        }
    }

    private static function archive(string $runDir, int $id): array
    {
        $manifest = self::readManifest($runDir, 'archive', $id);
        $compressor = CompressorFactory::create($manifest['compression']);
        $source = $manifest['source'];
        $part = fopen($manifest['part'], 'wb');
        if ($part === false) {
            throw new \RuntimeException("Unable to open part: {$manifest['part']}");
        }

        $files = [];
        try {
            foreach ($manifest['files'] as $index => $file) {
                $path = $source . '/' . $file['name'];
                $size = $file['size'];
                $partOffset = ftell($part);
                $tmp = $runDir . '/tmp-' . $id . '-' . $index;
                $attr = 0;
                $dataSize = 0;

                if ($size > 0) {
                    $compressor->compressFile($path, $tmp);
                    $compressedSize = filesize($tmp);
                    if ($compressedSize !== false && $compressedSize > 0
                        && $compressedSize < (int) ($size * self::COMPRESS_RATIO_THRESHOLD)) {
                        self::appendFile($part, $tmp);
                        $dataSize = $compressedSize;
                        $attr = $compressor->attr();
                    } else {
                        self::appendFile($part, $path);
                        $dataSize = (int) filesize($path);
                    }
                    @unlink($tmp);
                }

                $files[] = [
                    'name' => $file['name'],
                    'attr' => $attr,
                    'data_size' => $dataSize,
                    'part_offset' => $partOffset,
                ];
            }
        } finally {
            fclose($part);
        }

        return ['files' => $files];
    }

    private static function extract(string $runDir, int $id): array
    {
        $manifest = self::readManifest($runDir, 'extract', $id);
        $blob = fopen($manifest['blob'], 'rb');
        if ($blob === false) {
            throw new \RuntimeException("Unable to open blob: {$manifest['blob']}");
        }

        $compressor = CompressorFactory::create('zlib');
        $files = [];
        try {
            foreach ($manifest['files'] as $index => $file) {
                $dest = $manifest['dest'] . '/' . $file['name'];
                $tmp = $runDir . '/tmp-extract-' . $id . '-' . $index;
                $dataSize = $file['data_size'];

                if ($dataSize === 0) {
                    if (file_put_contents($dest, '') === false) {
                        throw new \RuntimeException("Unable to create: {$dest}");
                    }
                } else {
                    if (fseek($blob, $file['data_offset']) !== 0) {
                        throw new \RuntimeException("Unable to seek: {$file['name']}");
                    }
                    self::copyRange($blob, $tmp, $dataSize);
                    if (($file['attr'] & Metadata::ATTR_COMPRESS_ZSTD) !== 0) {
                        CompressorFactory::create('zstd')->decompressFile($tmp, $dest);
                    } elseif (($file['attr'] & Metadata::ATTR_COMPRESS_ZLIB) !== 0) {
                        $compressor->decompressFile($tmp, $dest);
                    } else {
                        if (!copy($tmp, $dest)) {
                            throw new \RuntimeException("Unable to copy: {$dest}");
                        }
                    }
                    @unlink($tmp);
                }

                chmod($dest, $file['mode'] & 0777);
                $files[] = ['name' => $file['name'], 'ok' => true];
            }
        } finally {
            fclose($blob);
        }

        return ['files' => $files];
    }

    private static function readManifest(string $runDir, string $action, int $id): array
    {
        $path = $runDir . '/' . $action . '-' . $id . '.json';
        $json = file_get_contents($path);
        if ($json === false) {
            throw new \RuntimeException("Unable to read manifest: {$path}");
        }

        return json_decode($json, true, 512, JSON_THROW_ON_ERROR);
    }

    private static function writeResult(string $path, array $result): void
    {
        $json = json_encode($result, JSON_THROW_ON_ERROR | JSON_UNESCAPED_SLASHES);
        $tmp = $path . '.tmp';
        if (file_put_contents($tmp, $json) === false || !rename($tmp, $path)) {
            throw new \RuntimeException("Unable to write result: {$path}");
        }
    }

    private static function appendFile($out, string $path): void
    {
        $in = fopen($path, 'rb');
        if ($in === false) {
            throw new \RuntimeException("Unable to open: {$path}");
        }
        try {
            if (stream_copy_to_stream($in, $out) === false) {
                throw new \RuntimeException("Unable to copy: {$path}");
            }
        } finally {
            fclose($in);
        }
    }

    private static function copyRange($in, string $path, int $length): void
    {
        $out = fopen($path, 'wb');
        if ($out === false) {
            throw new \RuntimeException("Unable to open: {$path}");
        }
        try {
            $remaining = $length;
            while ($remaining > 0) {
                $chunk = fread($in, min(self::CHUNK_SIZE, $remaining));
                if ($chunk === false || $chunk === '') {
                    throw new \RuntimeException('Unexpected end of blob data');
                }
                if (fwrite($out, $chunk) !== strlen($chunk)) {
                    throw new \RuntimeException("Unable to write: {$path}");
                }
                $remaining -= strlen($chunk);
            }
        } finally {
            fclose($out);
        }
    }

    public static function waitForStop(): void
    {
        while (true) {
            usleep(100000);
        }
    }
}
