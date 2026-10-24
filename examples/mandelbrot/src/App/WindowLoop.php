<?php

declare(strict_types=1);

namespace Swow\Mandelbrot\App;

use InvalidArgumentException;
use RuntimeException;
use Swow\Mandelbrot\Buffer;
use Swow\Mandelbrot\Config;
use Swow\Mandelbrot\Gui\Backend;
use Swow\Mandelbrot\Gui\Layout;
use Swow\Mandelbrot\View;
use Throwable;

use const STDERR;

/**
 * Window event loop, auto mode and the idle preview-to-full refine.
 */
final class WindowLoop
{
    private const PREVIEW_IDLE_SECONDS = 0.150;
    private const IDLE_TICK_MICROSECONDS = 2000;
    private const BUSY_TICK_MICROSECONDS = 200;

    private float $lastActivity = 0.0;
    private float $autoLastTick = 0.0;
    private ?string $pendingSave = null;
    private int $tickCount = 0;
    private bool $guiWaitFailed = false;

    public function __construct(
        private readonly Config $config,
        private readonly Backend $gui,
        private readonly View $view,
        private readonly Buffer $buffer,
        private readonly FramePipeline $pipeline,
        private readonly ServiceClient $service,
        private readonly Trace $trace,
    ) {
    }

    public function run(): int
    {
        try {
            $this->pipeline->startWorkers($this->config->threads());
            $this->syncFields();
            $this->gui->markInvalid(null, '');
            $this->enableAutoMode();
            $this->service->requestBuild(!$this->config->auto());
            $this->autoLastTick = microtime(true);
            $this->lastActivity = microtime(true);
            $this->trace->startHeartbeat();

            while (true) {
                $this->tickCount++;
                $tickStart = $this->trace->enabled() ? microtime(true) : 0.0;
                $events = $this->gui->pump();
                $pumpEnd = $this->trace->enabled() ? microtime(true) : 0.0;
                $quit = $this->handleEvents($events);
                $eventsEnd = $this->trace->enabled() ? microtime(true) : 0.0;

                if ($quit) {
                    // Stop idle workers from starting another band.
                    $this->pipeline->advanceGeneration();
                    $this->trace->tick(
                        $this->tickCount,
                        $tickStart,
                        $pumpEnd,
                        $eventsEnd,
                        $eventsEnd,
                        $eventsEnd,
                        $eventsEnd,
                    );
                    break;
                }

                if (
                    $this->config->auto() &&
                    $this->pipeline->pending() === 0 &&
                    $this->pipeline->frameDone() &&
                    !$this->service->busy()
                ) {
                    $now = microtime(true);
                    if (($now - $this->autoLastTick) * 1000.0 >= $this->config->autoMs()) {
                        $this->autoLastTick = $now;
                        $this->view->zoom(2.0);
                        $this->requestAutoFrame();
                    }
                }

                $this->service->poll();
                $this->service->assertThreadAlive();
                $this->pipeline->pollResults();
                if ($this->pipeline->pending() > 0 && !$this->pipeline->workersRunning()) {
                    throw new RuntimeException('workers exited before the frame completed');
                }
                $resultsEnd = $this->trace->enabled() ? microtime(true) : 0.0;

                if ($this->pipeline->frameDone()) {
                    if (
                        $this->pendingSave !== null &&
                        !$this->pipeline->framePreview() &&
                        !$this->service->busy()
                    ) {
                        $path = $this->pendingSave;
                        $this->pendingSave = null;
                        $this->service->requestSave($path);
                    } elseif (
                        $this->pipeline->framePreview() &&
                        !$this->service->busy() &&
                        !$this->config->auto() &&
                        microtime(true) - $this->lastActivity >= self::PREVIEW_IDLE_SECONDS
                    ) {
                        $wallStart = microtime(true);
                        $this->pipeline->startFrame(
                            false,
                            $this->pipeline->nextGeneration(),
                            $this->service->referenceVersion(),
                            $wallStart,
                        );
                        $this->gui->status('rendering');
                    }
                }

                $microseconds = $this->pipeline->pending() === 0
                    ? self::IDLE_TICK_MICROSECONDS
                    : self::BUSY_TICK_MICROSECONDS;
                $sleepStart = $this->trace->enabled() ? microtime(true) : 0.0;
                if ($this->guiWaitFailed) {
                    usleep($microseconds);
                } else {
                    try {
                        $this->gui->wait($microseconds);
                    } catch (Throwable) {
                        usleep($microseconds);
                        $this->guiWaitFailed = true;
                    }
                }
                $sleepEnd = $this->trace->enabled() ? microtime(true) : 0.0;

                $this->trace->tick(
                    $this->tickCount,
                    $tickStart,
                    $pumpEnd,
                    $eventsEnd,
                    $resultsEnd,
                    $sleepStart,
                    $sleepEnd,
                );
            }
        } finally {
            if (!$this->config->auto()) {
                try {
                    $this->gui->setBusy(false);
                } catch (Throwable) {
                }
            }
            // Hide the window before joining threads, so shutdown looks instant.
            try {
                $this->gui->close();
            } catch (Throwable $error) {
                fwrite(STDERR, 'mandelbrot: warning: window close failed: ' . $error->getMessage() . "\n");
            }
            $this->pipeline->stopWorkers(true);
            $this->service->stop(true);
        }

        return 0;
    }

    /**
     * @param list<array<string, mixed>> $events
     */
    private function handleEvents(array $events): bool
    {
        foreach ($events as $event) {
            switch ($event['type'] ?? '') {
                case 'zoomIn':
                    $this->trace->beginAction('zoomIn');
                    $this->view->zoom(2.0);
                    $this->requestPreview();
                    $this->trace->armAction($this->pipeline->generation());
                    break;
                case 'zoomOut':
                    $this->trace->beginAction('zoomOut');
                    $this->view->zoom(0.5);
                    $this->requestPreview();
                    $this->trace->armAction($this->pipeline->generation());
                    break;
                case 'panUp':
                    $this->pan(0, -1);
                    break;
                case 'panDown':
                    $this->pan(0, 1);
                    break;
                case 'panLeft':
                    $this->pan(1, 0);
                    break;
                case 'panRight':
                    $this->pan(-1, 0);
                    break;
                case 'reset':
                    $this->trace->beginAction('reset');
                    $this->view->reset();
                    $this->requestPreview();
                    $this->trace->armAction($this->pipeline->generation());
                    break;
                case 'apply':
                    $this->trace->beginAction('apply');
                    $this->trace->applyReceived(
                        (string) ($event['re'] ?? ''),
                        (string) ($event['im'] ?? ''),
                        (string) ($event['depth'] ?? ''),
                    );
                    $previousRe = $this->view->centerRe();
                    $previousIm = $this->view->centerIm();
                    $field = null;
                    $message = '';
                    try {
                        $this->view->setCenter($event['re'], $previousIm);
                    } catch (InvalidArgumentException $error) {
                        $field = 're';
                        $message = $error->getMessage();
                    }
                    if ($field === null) {
                        try {
                            $this->view->setCenter($event['re'], $event['im']);
                        } catch (InvalidArgumentException $error) {
                            $this->view->setCenter($previousRe, $previousIm);
                            $field = 'im';
                            $message = $error->getMessage();
                        }
                    }
                    if ($field === null) {
                        try {
                            $this->view->setDepth($event['depth']);
                        } catch (InvalidArgumentException $error) {
                            $this->view->setCenter($previousRe, $previousIm);
                            $field = 'depth';
                            $message = $error->getMessage();
                        }
                    }
                    if ($field !== null) {
                        $this->gui->markInvalid($field, $message);
                        $this->trace->cancelAction();
                        break;
                    }
                    $this->gui->markInvalid(null, '');
                    $this->requestPreview();
                    $this->trace->armAction($this->pipeline->generation());
                    break;
                case 'save':
                    $this->pendingSave = $this->gui->askSavePath($this->config->output());
                    break;
                case 'quit':
                    return true;
            }
        }

        return false;
    }

    private function pan(int $xSign, int $ySign): void
    {
        // One eighth of the visible area per press; screen y grows downwards.
        $dx = $xSign * ($this->buffer->fullWidth() / 8);
        $dy = $ySign * ($this->buffer->fullHeight() / 8);
        $this->view->pan($dx, $dy, $this->buffer->fullWidth(), $this->buffer->fullHeight());
        $this->requestPreview();
    }

    private function enableAutoMode(): void
    {
        if (!$this->config->auto()) {
            return;
        }

        $this->gui->setTitle(Layout::TITLE . ' (auto mode)');
        $this->gui->setBusy(true);
        $this->gui->setImageMode(false);
    }

    private function requestAutoFrame(): void
    {
        $this->syncFields();
        $this->service->requestBuild(false);
        $this->lastActivity = microtime(true);
    }

    private function requestPreview(): void
    {
        $this->syncFields();
        $this->service->requestBuild(true);
        $this->lastActivity = microtime(true);
    }

    private function syncFields(): void
    {
        $fields = $this->view->format();
        $this->gui->setFields($fields['re'], $fields['im'], $fields['depth']);
    }
}
