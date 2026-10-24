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
use Swow\Mandelbrot\Thread\Worker;
use Swow\Mandelbrot\View;
use Throwable;

/**
 * Reference build and PNG save requests to the service thread.
 *
 * Both requests are asynchronous; the caller either polls or waits for the
 * matching token.
 */
final class ServiceClient
{
    private const SERVICE_QUEUE_CAPACITY = 256;
    private const SERVICE_PUSH_TIMEOUT_MS = 1000;
    private const FRAME_TIMEOUT_SECONDS = 600.0;
    private const JOIN_TIMEOUT_MS = 5000;

    private ?SpscQueue $jobs = null;
    private ?SpscQueue $results = null;
    private ?Thread $thread = null;
    private bool $building = false;
    private bool $buildPreview = false;
    private bool $saving = false;
    private int $refVersion = 0;

    public function __construct(
        private readonly Config $config,
        private readonly ?Backend $gui,
        private readonly View $view,
        private readonly Buffer $buffer,
        Futex $generation,
        private readonly FramePipeline $pipeline,
        private readonly Trace $trace,
    ) {
        $this->jobs = new SpscQueue(self::SERVICE_QUEUE_CAPACITY);
        $this->results = new SpscQueue(self::SERVICE_QUEUE_CAPACITY);
        $this->thread = Worker::spawnService($buffer, $this->jobs, $this->results, $generation);
    }

    public function requestBuild(bool $preview): void
    {
        if ($this->jobs === null) {
            throw new RuntimeException('service thread is not running');
        }

        $gen = $this->pipeline->advanceGeneration();
        $this->refVersion++;
        $this->building = true;
        $this->buildPreview = $preview;

        if ($this->gui !== null) {
            $this->gui->setBusy(true);
            $this->gui->status('building reference');
        }

        try {
            $this->jobs->push([
                'type' => 'build',
                'gen' => $gen,
                'refVersion' => $this->refVersion,
                'limit' => $this->view->limit(),
                'view' => [
                    're' => $this->view->centerRe(),
                    'im' => $this->view->centerIm(),
                    'depth' => $this->view->depthText(),
                    'base' => $this->config->baseIterations(),
                    'width' => $this->buffer->fullWidth(),
                    'height' => $this->buffer->fullHeight(),
                ],
            ], self::SERVICE_PUSH_TIMEOUT_MS);
        } catch (SpscQueueException $error) {
            $this->building = false;
            throw new RuntimeException('reference build queue is full', 0, $error);
        }
    }

    public function requestSave(string $path): void
    {
        if ($this->jobs === null) {
            throw new RuntimeException('service thread is not running');
        }

        $this->saving = true;
        if ($this->gui !== null) {
            $this->gui->status('saving');
        }

        try {
            $this->jobs->push([
                'type' => 'save',
                'path' => $path,
                'width' => $this->buffer->fullWidth(),
                'height' => $this->buffer->fullHeight(),
            ], self::SERVICE_PUSH_TIMEOUT_MS);
        } catch (SpscQueueException $error) {
            $this->saving = false;
            throw new RuntimeException('save queue is full', 0, $error);
        }
    }

    public function save(string $path): void
    {
        $this->requestSave($path);
        $deadline = microtime(true) + self::FRAME_TIMEOUT_SECONDS;
        while ($this->saving) {
            $this->poll();
            if (!$this->saving) {
                return;
            }
            if ($this->thread === null || !$this->thread->isRunning()) {
                throw new RuntimeException('service thread exited before the save completed');
            }
            if (microtime(true) >= $deadline) {
                throw new RuntimeException('timed out waiting for the save');
            }
            usleep(1000);
        }
    }

    public function awaitBuild(): void
    {
        $deadline = microtime(true) + self::FRAME_TIMEOUT_SECONDS;
        while ($this->building) {
            $this->poll();
            if (!$this->building) {
                return;
            }
            if ($this->thread === null || !$this->thread->isRunning()) {
                throw new RuntimeException('service thread exited before the reference was ready');
            }
            if (microtime(true) >= $deadline) {
                throw new RuntimeException('timed out waiting for the reference');
            }
            usleep(1000);
        }
    }

    public function poll(): void
    {
        if ($this->results === null) {
            return;
        }

        while (!$this->results->isEmpty()) {
            $token = $this->results->pop(0);
            switch ($token['type'] ?? '') {
                case 'build':
                    $this->acceptBuildToken($token);
                    break;
                case 'save':
                    $this->acceptSaveToken($token);
                    break;
                default:
                    throw new RuntimeException('service error: ' . ($token['error'] ?? 'unknown'));
            }
        }
    }

    public function busy(): bool
    {
        return $this->building || $this->saving;
    }

    public function referenceVersion(): int
    {
        return $this->refVersion;
    }

    public function assertThreadAlive(): void
    {
        if (
            ($this->building || $this->saving)
            && $this->thread !== null
            && !$this->thread->isRunning()
        ) {
            throw new RuntimeException('service thread exited');
        }
    }

    public function stop(bool $quiet = false): void
    {
        $jobs = $this->jobs;
        // Keep the result queue alive until the service thread has exited.
        $results = $this->results;
        $thread = $this->thread;
        $this->jobs = null;
        $this->results = null;
        $this->thread = null;
        $this->building = false;
        $this->saving = false;

        if ($jobs === null || $thread === null) {
            return;
        }

        try {
            $jobs->push(['quit' => true], self::JOIN_TIMEOUT_MS);
        } catch (Throwable) {
        }

        try {
            $thread->wait(self::JOIN_TIMEOUT_MS);
            $status = $thread->getExitStatus();
            if ($status !== 0) {
                throw new RuntimeException("service thread exited with status {$status}");
            }
        } catch (Throwable $error) {
            if (!$quiet) {
                throw $error;
            }
            fwrite(
                STDERR,
                'mandelbrot: warning: service join failed ('
                    . $error->getMessage()
                    . "); process falls through without it\n",
            );
        }
    }

    /**
     * @param array<string, mixed> $token
     */
    private function acceptBuildToken(array $token): void
    {
        if (($token['gen'] ?? -1) !== $this->pipeline->generation() || !$this->building) {
            return;
        }

        $this->building = false;
        if (empty($token['ok'])) {
            throw new RuntimeException('reference build failed');
        }

        $this->pipeline->referenceBuilt((float) ($token['seconds'] ?? 0.0));
        $this->trace->applyReference($this->refVersion);

        $bands = $this->pipeline->startFrame(
            $this->buildPreview,
            $token['gen'],
            $this->refVersion,
            microtime(true),
        );
        $this->trace->applyDispatch($bands);

        if ($this->gui !== null) {
            $this->gui->status($this->buildPreview ? 'rendering preview' : 'rendering');
        }
    }

    /**
     * @param array<string, mixed> $token
     */
    private function acceptSaveToken(array $token): void
    {
        $this->saving = false;
        if (empty($token['ok'])) {
            $error = (string) ($token['error'] ?? 'unknown');
            if ($this->gui !== null) {
                $this->gui->status('save failed');
            }
            throw new RuntimeException('save failed: ' . $error);
        }

        if ($this->gui !== null) {
            $this->gui->status('saved');
        }
    }
}
