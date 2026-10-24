<?php

declare(strict_types=1);

namespace Swow\Examples\Transformer\Thread;

use RuntimeException;
use Swow\Examples\Gemm\Support\Threads;
use Swow\Examples\Transformer\Gguf\GgufFile;
use Swow\Examples\Transformer\Inference\Inferencer;
use Swow\Futex;
use Swow\SharedBuffer;
use Swow\Sync\ThreadBarrier;
use Swow\Thread;
use Throwable;

use function dirname;
use function max;

/*
 * Cooperative pool: workers run the whole forward and synchronize only with
 * futex and barriers. The parent writes one control block per token, wakes the
 * workers once, and waits at the token barrier.
 */
final class CooperativePool implements Inferencer
{
    public const COMMAND_FORWARD = 1;

    public const COMMAND_RESET = 2;

    public const COMMAND_QUIT = 3;

    public const MAX_ROWS = 262_144;

    public const BARRIER_TIMEOUT = 60_000;

    public const OFFSET_COMMAND = 0;

    public const OFFSET_TOKEN = 4;

    public const OFFSET_POSITION = 8;

    public const OFFSET_ERROR = 12;

    private SharedBuffer $control;

    private SharedBuffer $outputA;

    private SharedBuffer $outputB;

    private SharedBuffer $logits;

    private Futex $job;

    private ThreadBarrier $opBarrier;

    private ThreadBarrier $tokenBarrier;

    /** @var list<Thread> */
    private array $workers = [];

    private int $sequence = 0;

    private int $logitsRows;

    private int $threads;

    public function __construct(
        private string $ggufPath,
        int $threads,
    ) {
        $this->threads = max(1, $threads);

        $file = GgufFile::open($ggufPath);
        try {
            $this->logitsRows = $file->tensor('token_embd.weight')->rows();
        } finally {
            $file->close();
        }

        $this->control = new SharedBuffer(64);
        $this->outputA = new SharedBuffer(self::MAX_ROWS * 8);
        $this->outputB = new SharedBuffer(self::MAX_ROWS * 8);
        $this->logits = new SharedBuffer($this->logitsRows * 8);
        $this->job = new Futex(0);
        $this->opBarrier = new ThreadBarrier($this->threads);
        $this->tokenBarrier = new ThreadBarrier($this->threads + 1);

        $this->start();
    }

    public function __destruct()
    {
        $this->shutdown();
    }

    /** @return list<float> */
    public function forward(int $token, int $position): array
    {
        $this->submit(self::COMMAND_FORWARD, $token, $position);

        return $this->logits->viewF64(0, $this->logitsRows);
    }

    public function reset(): void
    {
        $this->submit(self::COMMAND_RESET);
    }

    private function submit(int $command, int $value = 0, int $extra = 0): void
    {
        $this->control->writeI32(self::OFFSET_ERROR, [0]);
        $this->control->writeI32(self::OFFSET_COMMAND, [$command, $value, $extra]);
        $this->sequence++;
        $this->job->setValue($this->sequence);
        $this->job->wake(-1);

        if (!$this->tokenBarrier->wait(self::BARRIER_TIMEOUT)) {
            throw new RuntimeException('Cooperative token barrier timed out');
        }
        if ($this->control->viewI32(self::OFFSET_ERROR, 1)[0] !== 0) {
            throw new RuntimeException('A cooperative worker failed');
        }
    }

    public function shutdown(): void
    {
        if ($this->workers === []) {
            return;
        }
        $this->control->writeI32(self::OFFSET_COMMAND, [self::COMMAND_QUIT]);
        $this->sequence++;
        $this->job->setValue($this->sequence);
        $this->job->wake(-1);
        foreach ($this->workers as $worker) {
            try {
                $worker->wait(30_000);
            } catch (Throwable) {
            }
        }
        $this->workers = [];
    }

    private function start(): void
    {
        $cpus = ThreadConfig::cpusFor($this->threads);
        try {
            for ($id = 0; $id < $this->threads; $id++) {
                $this->workers[] = Threads::spawn(
                    CooperativeWorker::class . '::run',
                    [
                        $id,
                        $this->threads,
                        $this->ggufPath,
                        $cpus[$id] ?? null,
                        $this->control,
                        $this->outputA,
                        $this->outputB,
                        $this->logits,
                        $this->job,
                        $this->opBarrier,
                        $this->tokenBarrier,
                    ],
                    dirname(__DIR__, 2) . '/autoload.php',
                );
            }
        } catch (Throwable $e) {
            $this->shutdown();
            throw $e;
        }
    }
}
