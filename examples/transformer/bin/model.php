<?php

declare(strict_types=1);

require dirname(__DIR__) . '/autoload.php';

use Swow\Examples\Transformer\Gguf\GgufArrayRef;
use Swow\Examples\Transformer\Gguf\GgufFile;

const MODEL_NAME = 'Qwen3.5-0.8B';
const MODEL_REPO = 'prithivMLmods/Qwen3.5-0.8B-f32-GGUF';

const MODEL_FILES = [
    'f32' => [
        'file' => 'Qwen3.5-0.8B.F32.gguf',
        'size' => 3020533664,
        'sha256' => '53d5c025769385c3c189cba43be43af53c4c8325dcf29497b44a6cadd8437c23',
    ],
    'q8' => [
        'file' => 'Qwen3.5-0.8B.Q8_0.gguf',
        'size' => 811843488,
        'sha256' => '367e9bd0383d7161898c996a58f9a36e6063585d92ae2e5be62e3def0ac1375c',
    ],
];

function usage(): void
{
    fwrite(STDERR, <<<'TXT'
usage: model.php <list|download|info> [options]

  list     print URLs, sizes and hashes
  download download the model files
  info     print GGUF metadata: model.php info <model.gguf> [--tensors]

download options:

  --source=hf|modelscope  download source, default hf
  --quant=f32|q8|both    quantizations, default both
  --output=DIR            output directory
  --proxy=URL             HTTP proxy, default HTTPS_PROXY or ALL_PROXY
  --token=TOKEN           access token, default HF_TOKEN or MODELSCOPE_API_TOKEN
  --force                 remove an existing file and download again

Output defaults to examples/transformer/models/qwen3.5-0.8b

TXT);
    exit(1);
}

/** @return array{source: string, quant: string, output: string, proxy: ?string, token: ?string, force: bool, list: bool} */
function parseOptions(array $arguments): array
{
    $options = [
        'source' => 'hf',
        'quant' => 'both',
        'output' => dirname(__DIR__) . '/models/qwen3.5-0.8b',
        'proxy' => null,
        'token' => null,
        'force' => false,
        'list' => false,
    ];

    foreach ($arguments as $argument) {
        if ($argument === '--force') {
            $options['force'] = true;
            continue;
        }
        if ($argument === '--list') {
            $options['list'] = true;
            continue;
        }
        if (!str_starts_with($argument, '--') || !str_contains($argument, '=')) {
            throw new InvalidArgumentException("Unknown argument: {$argument}");
        }
        [$name, $value] = explode('=', substr($argument, 2), 2);
        if (!array_key_exists($name, $options)) {
            throw new InvalidArgumentException("Unknown option: --{$name}");
        }
        $options[$name] = $value;
    }

    if (!in_array($options['source'], ['hf', 'modelscope'], true)) {
        throw new InvalidArgumentException('--source must be hf or modelscope');
    }
    if (!in_array($options['quant'], ['f32', 'q8', 'both'], true)) {
        throw new InvalidArgumentException('--quant must be f32, q8 or both');
    }

    $options['proxy'] ??= getenv('HTTPS_PROXY') ?: getenv('https_proxy') ?: getenv('ALL_PROXY') ?: getenv('all_proxy') ?: null;
    $options['token'] ??= $options['source'] === 'hf'
        ? (getenv('HF_TOKEN') ?: null)
        : (getenv('MODELSCOPE_API_TOKEN') ?: getenv('MODELSCOPE_TOKEN') ?: null);

    return $options;
}

function modelUrl(string $source, string $file): string
{
    return $source === 'hf'
        ? 'https://huggingface.co/' . MODEL_REPO . '/resolve/main/' . $file
        : 'https://modelscope.cn/models/' . MODEL_REPO . '/resolve/master/' . $file;
}

function formatBytes(int $bytes): string
{
    $units = ['B', 'KiB', 'MiB', 'GiB'];
    $value = (float) $bytes;
    foreach ($units as $unit) {
        if ($value < 1024.0 || $unit === 'GiB') {
            return sprintf('%.2f %s', $value, $unit);
        }
        $value /= 1024.0;
    }

    return sprintf('%d B', $bytes);
}

/** @param array{file: string, size: int, sha256: string} $entry */
function verifyFile(string $path, array $entry): bool
{
    if (!is_file($path) || filesize($path) !== $entry['size']) {
        return false;
    }
    printf('  checking sha256 ... ');
    $hash = hash_file('sha256', $path);
    if ($hash !== $entry['sha256']) {
        printf("FAIL\n  expected %s\n  actual   %s\n", $entry['sha256'], $hash);
        return false;
    }
    printf("ok\n");

    return true;
}

/** @param array{file: string, size: int, sha256: string} $entry */
function download(string $source, array $entry, string $directory, ?string $token, ?string $proxy, bool $force): void
{
    $path = $directory . '/' . $entry['file'];
    $part = $path . '.part';
    $url = modelUrl($source, $entry['file']);

    if (is_file($path) && !$force) {
        printf("%s: existing file\n", $entry['file']);
        if (verifyFile($path, $entry)) {
            return;
        }
        throw new RuntimeException("Existing file is invalid: {$path}; use --force to replace it");
    }

    if ($force) {
        @unlink($path);
        @unlink($part);
    }

    printf("%s: %s (%s)\n", $entry['file'], formatBytes($entry['size']), $url);
    $offset = is_file($part) ? filesize($part) : 0;
    if ($offset > $entry['size']) {
        throw new RuntimeException("Partial file is larger than expected: {$part}");
    }
    if ($offset === $entry['size']) {
        if (!verifyFile($part, $entry) || !rename($part, $path)) {
            throw new RuntimeException("Unable to finalize {$part}");
        }
        printf("  saved %s\n", $path);

        return;
    }

    $attempt = 0;
    while (true) {
        $attempt++;
        $file = fopen($part, 'c+b');
        if ($file === false) {
            throw new RuntimeException("Unable to open {$part}");
        }
        fseek($file, $offset);

        $curl = curl_init($url);
        if ($curl === false) {
            fclose($file);
            throw new RuntimeException('Unable to initialize curl');
        }

        $started = microtime(true);
        $lastReport = 0.0;
        $headers = ['Accept-Encoding: identity'];
        if ($token !== null && $token !== '') {
            $headers[] = 'Authorization: Bearer ' . $token;
        }
        curl_setopt_array($curl, [
            CURLOPT_FILE => $file,
            CURLOPT_FOLLOWLOCATION => true,
            CURLOPT_FAILONERROR => true,
            CURLOPT_CONNECTTIMEOUT => 30,
            CURLOPT_LOW_SPEED_LIMIT => 1024,
            CURLOPT_LOW_SPEED_TIME => 60,
            CURLOPT_HTTPHEADER => $headers,
            CURLOPT_USERAGENT => 'swow-transformer-model-downloader/1.0',
            CURLOPT_NOPROGRESS => false,
            CURLOPT_XFERINFOFUNCTION => static function (
                $resource,
                int $downloadTotal,
                int $downloaded,
            ) use ($offset, $entry, &$lastReport, &$started): int {
                $now = microtime(true);
                if ($now - $lastReport >= 1.0) {
                    $lastReport = $now;
                    $current = $offset + $downloaded;
                    $percent = $entry['size'] > 0 ? $current * 100.0 / $entry['size'] : 0.0;
                    $rate = $now > $started ? $downloaded / ($now - $started) : 0.0;
                    printf(
                        "\r  %6.2f%%  %s / %s  %s/s",
                        $percent,
                        formatBytes($current),
                        formatBytes($entry['size']),
                        formatBytes((int) $rate),
                    );
                }

                return 0;
            },
        ]);
        if ($offset > 0) {
            curl_setopt($curl, CURLOPT_RANGE, $offset . '-');
        }
        if ($proxy !== null && $proxy !== '') {
            curl_setopt($curl, CURLOPT_PROXY, $proxy);
        }

        $ok = curl_exec($curl);
        $status = (int) curl_getinfo($curl, CURLINFO_RESPONSE_CODE);
        $error = curl_error($curl);
        unset($curl);
        fclose($file);
        printf("\n");

        if ($ok === false) {
            throw new RuntimeException("Download failed: {$error}");
        }
        if ($status === 200 && $offset > 0) {
            // The server ignored Range; restart without appending to the partial file.
            if ($attempt > 1) {
                throw new RuntimeException('Server does not support resume');
            }
            @unlink($part);
            $offset = 0;
            continue;
        }
        if ($status !== 200 && $status !== 206) {
            throw new RuntimeException("Unexpected HTTP status {$status}");
        }

        clearstatcache(true, $part);
        $current = is_file($part) ? filesize($part) : 0;
        if ($current < $entry['size']) {
            if ($current > $offset && $attempt < 5) {
                $offset = $current;
                continue;
            }
            throw new RuntimeException("Incomplete download: {$current} of {$entry['size']} bytes");
        }
        if ($current > $entry['size']) {
            throw new RuntimeException("Downloaded file is larger than expected: {$path}");
        }

        if (!verifyFile($part, $entry)) {
            throw new RuntimeException("sha256 mismatch: {$part}");
        }
        if (!rename($part, $path)) {
            throw new RuntimeException("Unable to rename {$part} to {$path}");
        }
        printf("  saved %s\n", $path);

        return;
    }
}

function downloadModels(array $arguments, bool $listOnly = false): void
{
    $options = parseOptions($arguments);
    $options['list'] = $options['list'] || $listOnly;
    $quants = $options['quant'] === 'both' ? ['f32', 'q8'] : [$options['quant']];

    if ($options['list']) {
        foreach ($quants as $quant) {
            $entry = MODEL_FILES[$quant];
            printf(
                "%s  %s  %s  %s\n",
                $quant,
                formatBytes($entry['size']),
                $entry['sha256'],
                modelUrl($options['source'], $entry['file']),
            );
        }

        return;
    }

    if (!extension_loaded('curl')) {
        throw new RuntimeException('The curl extension is required');
    }
    if (!is_dir($options['output']) && !mkdir($options['output'], 0755, true) && !is_dir($options['output'])) {
        throw new RuntimeException("Unable to create {$options['output']}");
    }

    printf("%s (%s)\n", MODEL_NAME, $options['source']);
    foreach ($quants as $quant) {
        download($options['source'], MODEL_FILES[$quant], $options['output'], $options['token'], $options['proxy'], $options['force']);
    }
    printf("done: %s\n", $options['output']);
}

function modelInfo(array $arguments): void
{
    $path = $arguments[0] ?? null;
    if ($path === null) {
        usage();
    }
    $file = GgufFile::open($path);
    try {
        printf("version: %d\n", $file->version());
        printf("file: %s\n", $file->path());
        printf("size: %d bytes\n", $file->fileSize());
        printf("data_start: %d\n", $file->dataStart());
        printf("alignment: %d\n", $file->alignment());
        printf("tensors: %d\n", count($file->tensors()));
        printf("metadata: %d\n", count($file->metadata()->all()));

        $metadata = $file->metadata();
        foreach (['general.architecture', 'general.name', 'qwen35.block_count', 'qwen35.embedding_length', 'qwen35.feed_forward_length', 'qwen35.attention.head_count', 'qwen35.attention.head_count_kv'] as $key) {
            if ($metadata->has($key)) {
                printf("%s: %s\n", $key, (string) $metadata->get($key));
            }
        }

        $tokens = $metadata->get('tokenizer.ggml.tokens');
        if ($tokens instanceof GgufArrayRef) {
            printf("tokenizer.ggml.tokens: lazy array count=%d\n", $tokens->count());
        }

        if (in_array('--tensors', $arguments, true)) {
            printf("\n%-48s %-6s %-20s %12s\n", 'name', 'type', 'shape', 'bytes');
            foreach ($file->tensors() as $tensor) {
                printf(
                    "%-48s %-6s %-20s %12d\n",
                    $tensor->name(),
                    $tensor->typeName(),
                    implode('x', $tensor->dimensions()),
                    $tensor->byteSize(),
                );
            }
        }
    } finally {
        $file->close();
    }
}

$arguments = array_slice($argv, 1);
$command = array_shift($arguments);

try {
    match ($command) {
        'list' => downloadModels($arguments, true),
        'download' => downloadModels($arguments),
        'info' => modelInfo($arguments),
        default => usage(),
    };
} catch (Throwable $e) {
    fwrite(STDERR, $e::class . ': ' . $e->getMessage() . "\n");
    exit(1);
}
