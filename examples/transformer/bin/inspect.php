<?php

declare(strict_types=1);

require dirname(__DIR__) . '/autoload.php';

use Swow\Examples\Transformer\Gguf\GgufFile;
use Swow\Examples\Transformer\Reference\Logits;
use Swow\Examples\Transformer\Sampler\Sampler;
use Swow\Examples\Transformer\Sampler\SamplingConfig;
use Swow\Examples\Transformer\Sampler\SamplingContext;
use Swow\Examples\Transformer\Tokenizer\QwenTokenizer;

function usage(): void
{
    fwrite(STDERR, <<<'TXT'
usage: inspect.php <tokenize|sample|compare> ...

  tokenize <model.gguf> <text>
      encode and decode the text

  sample <logits.bin> [--greedy] [--temp=N] [--top-k=N] [--top-p=N] [--min-p=N] [--seed=N]
      sample one token from a logits dump

  compare <reference.bin> <actual.bin> [--top=N]
      compare two logits dumps

TXT);
    exit(1);
}

/** @param list<string> $arguments */
function tokenize(array $arguments): void
{
    [$path, $text] = $arguments + [null, null];
    if ($path === null || $text === null) {
        usage();
    }
    $file = GgufFile::open($path);
    try {
        $tokenizer = QwenTokenizer::fromGguf($file);
        $ids = $tokenizer->encode($text);
        printf("ids: %s\n", json_encode($ids, JSON_THROW_ON_ERROR));
        printf("decoded: %s\n", json_encode($tokenizer->decode($ids), JSON_THROW_ON_ERROR));
    } finally {
        $file->close();
    }
}

/** @param list<string> $arguments */
function sample(array $arguments): void
{
    $path = $arguments[0] ?? null;
    if ($path === null) {
        usage();
    }
    $config = new SamplingConfig();
    foreach (array_slice($arguments, 1) as $argument) {
        if ($argument === '--greedy') {
            $config->temperature = 0.0;
            continue;
        }
        if (!str_starts_with($argument, '--') || !str_contains($argument, '=')) {
            usage();
        }
        [$key, $value] = explode('=', substr($argument, 2), 2);
        match ($key) {
            'temp' => $config->temperature = (float) $value,
            'top-k' => $config->topK = (int) $value,
            'top-p' => $config->topP = (float) $value,
            'min-p' => $config->minP = (float) $value,
            'seed' => $config->seed = (int) $value,
            default => usage(),
        };
    }

    $sampler = new Sampler($config);
    printf("token=%d\n", $sampler->sample(Logits::read($path), new SamplingContext()));
}

/** @param list<string> $arguments */
function compare(array $arguments): void
{
    [$referencePath, $actualPath] = $arguments + [null, null];
    if ($referencePath === null || $actualPath === null) {
        usage();
    }
    $top = 5;
    foreach (array_slice($arguments, 2) as $argument) {
        if (str_starts_with($argument, '--top=')) {
            $top = max(1, (int) substr($argument, 6));
        }
    }

    $result = Logits::compare(Logits::read($referencePath), Logits::read($actualPath), $top);
    printf("count: %d\n", $result['count']);
    printf("max_abs: %.6e\n", $result['max_abs']);
    printf("max_rel: %.6e\n", $result['max_rel']);
    printf("top_reference: %s\n", implode(', ', $result['top_reference']));
    printf("top_actual:    %s\n", implode(', ', $result['top_actual']));
    printf("top1_match: %s\n", $result['top1_match'] ? 'yes' : 'no');
    printf("top5_match: %s\n", $result['top5_match'] ? 'yes' : 'no');
}

$arguments = array_slice($argv, 1);
$command = array_shift($arguments);

try {
    match ($command) {
        'tokenize' => tokenize($arguments),
        'sample' => sample($arguments),
        'compare' => compare($arguments),
        default => usage(),
    };
} catch (Throwable $e) {
    fwrite(STDERR, $e::class . ': ' . $e->getMessage() . "\n");
    exit(1);
}
