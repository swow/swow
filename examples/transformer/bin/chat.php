<?php

declare(strict_types=1);

require dirname(__DIR__) . '/autoload.php';

use Swow\Examples\Transformer\Chat\Conversation;
use Swow\Examples\Transformer\Gguf\GgufFile;
use Swow\Examples\Transformer\Sampler\Sampler;
use Swow\Examples\Transformer\Sampler\SamplingConfig;
use Swow\Examples\Transformer\Terminal\ChatTui;
use Swow\Examples\Transformer\Terminal\XtermTerminal;
use Swow\Examples\Transformer\Thread\CooperativePool;
use Swow\Examples\Transformer\Thread\ThreadConfig;
use Swow\Examples\Transformer\Tokenizer\QwenTokenizer;

if ($argc < 2) {
    fwrite(STDERR, "usage: chat.php <model.gguf> [options]\n");
    fwrite(STDERR, "  --context=N       context token limit, default 2048\n");
    fwrite(STDERR, "  --tokens=N        max new tokens per turn, default 32\n");
    fwrite(STDERR, "  --system=TEXT     system message, default \"You are a helpful assistant\"\n");
    fwrite(STDERR, "  --no-system       disable the system message\n");
    fwrite(STDERR, "  --thinking        enable Qwen thinking prompt\n");
    fwrite(STDERR, "  --greedy          greedy sampling\n");
    fwrite(STDERR, "  --threads=N       inference threads, default physical cores\n");
    fwrite(STDERR, "  --no-pin          disable physical-core pinning\n");
    fwrite(STDERR, "  --temp=N --top-k=N --top-p=N --min-p=N --seed=N\n");
    exit(1);
}

$modelPath = $argv[1];
$contextLimit = 2048;
$maxTokens = 32;
$system = 'You are a helpful assistant';
$thinking = false;
$threads = ThreadConfig::defaultThreads();
$pinned = true;
$config = new SamplingConfig();
foreach (array_slice($argv, 2) as $argument) {
    if ($argument === '--thinking') {
        $thinking = true;
        continue;
    }
    if ($argument === '--greedy') {
        $config->temperature = 0.0;
        continue;
    }
    if ($argument === '--no-system') {
        $system = null;
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
        'context', 'ctx' => $contextLimit = max(1, (int) $value),
        'tokens', 'max-tokens' => $maxTokens = max(1, (int) $value),
        'system' => $system = $value,
        'threads' => $threads = max(1, (int) $value),
        'temp' => $config->temperature = (float) $value,
        'top-k' => $config->topK = (int) $value,
        'top-p' => $config->topP = (float) $value,
        'min-p' => $config->minP = (float) $value,
        'seed' => $config->seed = (int) $value,
        default => throw new RuntimeException("unknown option: {$key}"),
    };
}

if ($threads > 1 && !class_exists(Swow\Thread::class)) {
    fwrite(STDERR, "warning: Swow Thread is not loaded, using one thread\n");
    $threads = 1;
}
ThreadConfig::setThreads($threads);
ThreadConfig::setPinned($pinned);

ini_set('memory_limit', '-1');
$terminal = new XtermTerminal();
$file = GgufFile::open($modelPath);
$tokenizer = QwenTokenizer::fromGguf($file);
$pool = new CooperativePool($modelPath, $threads);
$sampler = new Sampler($config);
$conversation = new Conversation(
    $pool,
    $tokenizer,
    $sampler,
    $contextLimit,
    $maxTokens,
    $thinking,
    $system,
);

try {
    (new ChatTui($terminal, $conversation, $system))->run();
} finally {
    $pool->shutdown();
    $file->close();
}
