<?php

declare(strict_types=1);

require dirname(__DIR__) . '/autoload.php';

use Swow\Examples\Transformer\Gguf\GgufFile;
use Swow\Examples\Transformer\Sampler\Sampler;
use Swow\Examples\Transformer\Sampler\SamplingConfig;
use Swow\Examples\Transformer\Sampler\SamplingContext;
use Swow\Examples\Transformer\Thread\CooperativePool;
use Swow\Examples\Transformer\Thread\ThreadConfig;
use Swow\Examples\Transformer\Tokenizer\QwenTokenizer;

if ($argc < 3) {
    fwrite(STDERR, "usage: generate.php <model.gguf> --prompt=TEXT [options]\n");
    fwrite(STDERR, "  --prompt=TEXT       inline prompt\n");
    fwrite(STDERR, "  --prompt-file=FILE  read the prompt from a file\n");
    fwrite(STDERR, "  --ids=100,101       run raw token ids instead of a prompt\n");
    fwrite(STDERR, "  --tokens=N          max generated tokens, default 8; 0 only forwards\n");
    fwrite(STDERR, "  --output=PREFIX     save logits as PREFIX_logits_NN.bin\n");
    fwrite(STDERR, "  --threads=N         worker threads, default physical cores\n");
    fwrite(STDERR, "  --no-pin            disable CPU pinning\n");
    fwrite(STDERR, "  --greedy            greedy sampling\n");
    fwrite(STDERR, "  --temp=N --top-k=N --top-p=N --min-p=N --seed=N\n");
    exit(1);
}

$modelPath = $argv[1];
$prompt = null;
$ids = null;
$output = null;
$maxTokens = 8;
$threads = ThreadConfig::defaultThreads();
$pinned = true;
$config = new SamplingConfig();
foreach (array_slice($argv, 2) as $argument) {
    if ($argument === '--greedy') {
        $config->temperature = 0.0;
        continue;
    }
    if ($argument === '--no-pin') {
        $pinned = false;
        continue;
    }
    if (!str_starts_with($argument, '--') || !str_contains($argument, '=')) {
        fwrite(STDERR, "unknown argument: {$argument}\n");
        exit(1);
    }
    [$key, $value] = explode('=', substr($argument, 2), 2);
    match ($key) {
        'prompt-file' => $prompt = file_get_contents($value),
        'prompt' => $prompt = $value,
        'ids' => $ids = array_map(static fn(string $id): int => (int) $id, explode(',', $value)),
        'output' => $output = $value,
        'tokens', 'max-tokens' => $maxTokens = max(0, (int) $value),
        'threads' => $threads = max(1, (int) $value),
        'temp' => $config->temperature = (float) $value,
        'top-k' => $config->topK = (int) $value,
        'top-p' => $config->topP = (float) $value,
        'min-p' => $config->minP = (float) $value,
        'seed' => $config->seed = (int) $value,
        default => throw new RuntimeException("unknown option: {$key}"),
    };
}
if ($ids === null && ($prompt === false || $prompt === null)) {
    fwrite(STDERR, "prompt or ids are required\n");
    exit(1);
}
ThreadConfig::setThreads($threads);
ThreadConfig::setPinned($pinned);

ini_set('memory_limit', '-1');
$started = microtime(true);
$file = GgufFile::open($modelPath);
$tokenizer = QwenTokenizer::fromGguf($file);
$sampler = new Sampler($config);
$promptIds = $ids ?? $tokenizer->encode($prompt);
$context = new SamplingContext(history: $promptIds);
$stopTokens = [$tokenizer->eosId()];
if ($tokenizer->padId() !== null) {
    $stopTokens[] = $tokenizer->padId();
}

$pool = new CooperativePool($modelPath, $threads);
printf(
    "[setup] threads=%d pinned=%d prompt_tokens=%d\n",
    $threads,
    $pinned ? 1 : 0,
    count($promptIds),
);

$logits = null;
$position = 0;
foreach ($promptIds as $index => $token) {
    $startedStep = microtime(true);
    $logits = $pool->forward($token, $position);
    printf(
        "[input %d/%d] pos=%d token=%d top=%d step=%.3fs total=%.3fs\n",
        $index + 1,
        count($promptIds),
        $position,
        $token,
        topToken($logits),
        microtime(true) - $startedStep,
        microtime(true) - $started,
    );
    saveLogits($output, $position, $logits);
    $position++;
}

if ($logits === null) {
    fwrite(STDERR, "empty prompt\n");
    exit(1);
}

$assistant = '';
for ($generated = 1; $generated <= $maxTokens; $generated++) {
    $token = $sampler->sample($logits, $context);
    if (in_array($token, $stopTokens, true)) {
        printf("[stop] token=%d total=%.3fs\n", $token, microtime(true) - $started);
        break;
    }
    $text = $tokenizer->decode([$token]);
    $assistant .= $text;
    $context->history[] = $token;
    printf(
        "[gen %d] token=%d text=%s total=%.3fs\n",
        $generated,
        $token,
        json_encode($text, JSON_INVALID_UTF8_SUBSTITUTE | JSON_THROW_ON_ERROR),
        microtime(true) - $started,
    );
    $logits = $pool->forward($token, $position);
    saveLogits($output, $position, $logits);
    $position++;
}

printf(
    "[result] text=%s total=%.3fs\n",
    json_encode($assistant, JSON_INVALID_UTF8_SUBSTITUTE | JSON_THROW_ON_ERROR),
    microtime(true) - $started,
);

$pool->shutdown();
$file->close();

/** @param list<float> $logits */
function topToken(array $logits): int
{
    $top = 0;
    $maximum = -INF;
    foreach ($logits as $id => $value) {
        if ($value > $maximum) {
            $maximum = $value;
            $top = $id;
        }
    }

    return $top;
}

/** @param list<float> $logits */
function saveLogits(?string $prefix, int $position, array $logits): void
{
    if ($prefix === null) {
        return;
    }
    $path = sprintf('%s_logits_%02d.bin', $prefix, $position);
    file_put_contents($path, pack('g*', ...$logits));
    printf("[dump] %s\n", $path);
}
