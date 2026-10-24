<?php

declare(strict_types=1);

namespace Swow\Mandelbrot\App;

use RuntimeException;
use Swow\Futex;
use Swow\SpscQueue;
use Swow\SpscQueueException;
use Swow\Thread;
use Swow\Mandelbrot\Buffer;
use Swow\Mandelbrot\Config;
use Swow\Mandelbrot\Gui\Backend;
use Swow\Mandelbrot\Renderer;
use Swow\Mandelbrot\Thread\Worker;
use Swow\Mandelbrot\View;
use Throwable;

/**
 * Band dispatch, result tokens and presentation.
 *
 * Frames are split into Buffer::BAND_ROWS bands and spread over the worker
 * queues. Each band token either updates the frame state or is dropped as
 * stale.
 */
final class FramePipeline
{
    private const WORKER_QUEUE_CAPACITY = 256;
    private const TASK_PUSH_TIMEOUT_MS = 1000;
    private const FRAME_TIMEOUT_SECONDS = 600.0;
    private const JOIN_TIMEOUT_MS = 5000;

    /** @var list<SpscQueue> */
    private array $taskQueues = [];

    /** @var list<SpscQueue> */
    private array $resultQueues = [];

    /** @var list<Thread> */
    private array $workers = [];

    /** @var array<int, array{rowBegin: int, rowEnd: int, preview: bool}> */
    private array $dispatchedBands = [];

    private int $workerCount = 0;
    private int $gen = 0;
    private int $refVersion = 0;
    private int $pending = 0;
    private bool $frameDone = true;
    private bool $framePreview = false;

    /** @var array<string, mixed> */
    private array $frameParams = [];

    private float $frameStartTime = 0.0;
    private float $frameWallStart = 0.0;
    private float $lastPreviewWall = 0.0;
    private float $lastFullWall = 0.0;
    private float $nextReferenceSeconds = 0.0;
    private float $frameReferenceSeconds = 0.0;
    private int $frameBands = 0;
    private int $frameGlitches = 0;

    public function __construct(
        private readonly Config $config,
        private readonly ?Backend $gui,
        private readonly View $view,
        private readonly Buffer $buffer,
        private readonly Futex $generation,
        private readonly Trace $trace,
    ) {
    }

    public function startWorkers(int $threads): void
    {
        $threads = max(1, $threads);
        $this->taskQueues = [];
        $this->resultQueues = [];
        for ($i = 0; $i < $threads; $i++) {
            $this->taskQueues[] = new SpscQueue(self::WORKER_QUEUE_CAPACITY);
            $this->resultQueues[] = new SpscQueue(self::WORKER_QUEUE_CAPACITY);
        }

        $this->workers = [];
        foreach ($this->taskQueues as $index => $tasks) {
            $this->workers[] = Worker::spawn(
                $this->buffer,
                $tasks,
                $this->resultQueues[$index],
                $this->generation,
            );
        }
        $this->workerCount = $threads;
    }

    public function stopWorkers(bool $quiet = false): void
    {
        $queues = $this->taskQueues;
        // Keep the result queues alive until every worker has exited; a worker
        // still holds and may push to them while the quit is in flight.
        $resultQueues = $this->resultQueues;
        $workers = $this->workers;
        $this->taskQueues = [];
        $this->resultQueues = [];
        $this->workers = [];
        $this->workerCount = 0;

        foreach ($queues as $tasks) {
            try {
                $tasks->push(['quit' => true], self::JOIN_TIMEOUT_MS);
            } catch (SpscQueueException) {
            }
        }

        $failure = null;
        foreach ($workers as $index => $worker) {
            try {
                $worker->wait(self::JOIN_TIMEOUT_MS);
                $status = $worker->getExitStatus();
                if ($status !== 0 && $failure === null) {
                    $failure = new RuntimeException("worker {$index} exited with status {$status}");
                }
            } catch (Throwable $error) {
                if ($failure === null) {
                    $failure = $error;
                }
            }
        }

        if ($failure !== null) {
            if ($quiet) {
                fwrite(
                    STDERR,
                    'mandelbrot: warning: worker join failed ('
                        . $failure->getMessage()
                        . "); process falls through without all workers\n",
                );

                return;
            }

            throw $failure;
        }
    }

    /**
     * Advance the generation and drop queued work for the old one.
     */
    public function advanceGeneration(): int
    {
        $this->generation->setValue(++$this->gen);
        $this->drainTasks();

        return $this->gen;
    }

    /**
     * The next generation, published by startFrame(). Used by the idle refine,
     * which reuses the reference orbit and does not go through the service.
     */
    public function nextGeneration(): int
    {
        return ++$this->gen;
    }

    public function generation(): int
    {
        return $this->gen;
    }

    public function workerCount(): int
    {
        return $this->workerCount;
    }

    public function pending(): int
    {
        return $this->pending;
    }

    public function frameDone(): bool
    {
        return $this->frameDone;
    }

    public function framePreview(): bool
    {
        return $this->framePreview;
    }

    /**
     * @return array<string, mixed>
     */
    public function frameParams(): array
    {
        return $this->frameParams;
    }

    public function referenceBuilt(float $seconds): void
    {
        $this->nextReferenceSeconds = $seconds;
    }

    public function startFrame(bool $preview, int $gen, int $refVersion, float $wallStart): int
    {
        $this->gen = $gen;
        $this->refVersion = $refVersion;
        $this->generation->setValue($gen);
        $this->framePreview = $preview;
        $this->frameDone = false;
        $this->frameWallStart = $wallStart;

        if ($this->gui !== null) {
            $this->gui->setBusy(true);
            if ($preview) {
                // The full photo stays hidden while preview bands overwrite it.
                $this->gui->setImageMode(true);
            }
        }

        $width = $this->buffer->fullWidth();
        $height = $this->buffer->fullHeight();
        $this->frameParams = $preview
            ? $this->view->params($width, $height, $this->buffer->previewStep())
            : $this->view->params($width, $height, 1);
        // Thumbnails may use the fast direct path; the final image is exact.
        $this->frameParams['path'] = $preview
            ? ($this->view->depthFloat() < Renderer::DIRECT_DEPTH ? 'direct' : 'perturb')
            : 'perturb';

        $this->drainTasks();
        $this->dispatchedBands = [];
        $this->pending = $this->dispatch($this->frameParams, $preview);
        $this->frameDone = $this->pending === 0;
        $this->frameStartTime = microtime(true);
        $this->frameReferenceSeconds = $this->nextReferenceSeconds;
        $this->nextReferenceSeconds = 0.0;
        $this->frameBands = $this->pending;
        $this->frameGlitches = 0;

        return $this->pending;
    }

    /**
     * @param array<string, mixed> $params
     */
    private function dispatch(array $params, bool $preview): int
    {
        $queueCount = count($this->taskQueues);
        if ($queueCount === 0) {
            throw new RuntimeException('no worker queues');
        }

        $height = $params['height'];
        $bands = 0;
        for ($row = 0; $row < $height; $row += Buffer::BAND_ROWS) {
            $end = min($row + Buffer::BAND_ROWS, $height);
            $bandIndex = intdiv($row, Buffer::BAND_ROWS);
            $this->dispatchedBands[$bandIndex] = [
                'rowBegin' => $row,
                'rowEnd' => $end,
                'preview' => $preview,
            ];
            try {
                $this->taskQueues[$bands % $queueCount]->push([
                    'gen' => $this->gen,
                    'rowBegin' => $row,
                    'rowEnd' => $end,
                    'refVersion' => $this->refVersion,
                    'preview' => $preview,
                    'view' => [
                        're' => $this->view->centerRe(),
                        'im' => $this->view->centerIm(),
                        'depth' => $this->view->depthText(),
                        'base' => $this->config->baseIterations(),
                    ],
                    'params' => $params,
                ], self::TASK_PUSH_TIMEOUT_MS);
            } catch (SpscQueueException $error) {
                throw new RuntimeException('worker task queue is full', 0, $error);
            }
            $bands++;
        }

        return $bands;
    }

    private function drainTasks(): void
    {
        foreach ($this->taskQueues as $tasks) {
            $tasks->drain();
        }
    }

    public function pollResults(): void
    {
        if ($this->pending === 0) {
            return;
        }

        foreach ($this->resultQueues as $results) {
            while (!$results->isEmpty()) {
                $this->acceptToken($results->pop(0));
                if ($this->pending === 0) {
                    return;
                }
            }
        }
    }

    public function awaitFrame(): void
    {
        $deadline = microtime(true) + self::FRAME_TIMEOUT_SECONDS;
        while (!$this->frameDone) {
            $this->pollResults();
            if ($this->frameDone) {
                return;
            }
            if (!$this->workersRunning()) {
                throw new RuntimeException('workers exited before the frame completed');
            }
            if (microtime(true) >= $deadline) {
                throw new RuntimeException('timed out waiting for the frame');
            }
            usleep(1000);
        }
    }

    /**
     * @param array<string, mixed> $token
     */
    private function acceptToken(array $token): void
    {
        if ($token['gen'] !== $this->gen) {
            return;
        }

        $bandIndex = $token['bandIndex'];
        if (!isset($this->dispatchedBands[$bandIndex])) {
            return;
        }
        $band = $this->dispatchedBands[$bandIndex];
        unset($this->dispatchedBands[$bandIndex]);

        $this->trace->firstResult($token['gen']);

        $this->frameGlitches += $token['glitchCount'];
        if ($token['overflow']) {
            fwrite(STDERR, sprintf(
                "mandelbrot: warning: band %d reported %d glitches, repair skipped\n",
                $bandIndex,
                $token['glitchCount'],
            ));
            fflush(STDERR);
        }

        if ($this->gui !== null) {
            $bytes = $this->buffer->readRows(
                $band['preview'],
                $band['rowBegin'],
                $band['rowEnd'],
            );
            $this->gui->presentRows(
                $bytes,
                $band['rowBegin'],
                $band['rowEnd'],
                $this->frameParams['width'],
                $band['preview'],
            );
        }

        $this->pending--;
        if ($this->pending === 0) {
            $this->frameDone = true;
            $this->logFrameStats();
            if ($this->gui !== null) {
                // Reveal the full photo only after every band is in place.
                if (!$band['preview']) {
                    if (!$this->config->auto()) {
                        $this->gui->setImageMode(false);
                        $this->gui->setBusy(false);
                    }
                }
                $this->gui->status($this->readyStatus());
            }
        }
    }

    public function workersRunning(): bool
    {
        foreach ($this->workers as $worker) {
            if ($worker->isRunning()) {
                return true;
            }
        }

        return false;
    }

    private function logFrameStats(): void
    {
        $now = microtime(true);
        if ($this->framePreview) {
            $this->lastPreviewWall = $now - $this->frameWallStart;
        } else {
            $this->lastFullWall = $now - $this->frameWallStart;
        }
        fprintf(
            STDERR,
            "frame %d kind=%s depth=%.6g full=%.3fs wall=%.3fs glitches=%d bands=%d reference=%.3fs\n",
            $this->gen,
            $this->framePreview ? 'preview' : 'full',
            $this->view->depthFloat(),
            $now - $this->frameStartTime,
            $now - $this->frameWallStart,
            $this->frameGlitches,
            $this->frameBands,
            $this->frameReferenceSeconds,
        );
        fflush(STDERR);
    }

    private function readyStatus(): string
    {
        $parts = ['ready'];
        if ($this->lastPreviewWall > 0.0) {
            $parts[] = sprintf('preview %.2fs', $this->lastPreviewWall);
        }
        if ($this->lastFullWall > 0.0) {
            $parts[] = sprintf('full %.2fs', $this->lastFullWall);
        }

        return implode('  ', $parts);
    }
}
