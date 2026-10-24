<?php

declare(strict_types=1);

namespace Swow\Mandelbrot\App;

/**
 * MANDELBROT_TRACE output.
 *
 * Tick lines carry the per-phase timings of the window loop; apply lines
 * follow one user action from the click to its first result.
 */
final class Trace
{
    private int $heartbeatTicks = 0;
    private float $heartbeatSince = 0.0;
    private float $heartbeatPump = 0.0;
    private float $heartbeatEvents = 0.0;
    private float $heartbeatResults = 0.0;
    private float $heartbeatSleep = 0.0;

    private ?string $actionKind = null;
    private float $actionClickTime = 0.0;
    private int $actionGen = -1;
    private bool $actionFirstResult = false;

    private readonly bool $enabled;

    public function __construct()
    {
        $trace = getenv('MANDELBROT_TRACE');
        $this->enabled = $trace !== false && $trace !== '' && $trace !== '0';
    }

    public function enabled(): bool
    {
        return $this->enabled;
    }

    public function startHeartbeat(): void
    {
        $this->heartbeatSince = $this->enabled ? microtime(true) : 0.0;
    }

    private function message(string $message): void
    {
        fwrite(STDERR, 'mandelbrot: trace: ' . $message . "\n");
        fflush(STDERR);
    }

    public function beginAction(string $kind): void
    {
        if (!$this->enabled) {
            return;
        }

        $this->actionKind = $kind;
        $this->actionClickTime = microtime(true);
        $this->actionGen = -1;
        $this->actionFirstResult = false;
    }

    public function cancelAction(): void
    {
        $this->actionKind = null;
    }

    public function armAction(int $gen): void
    {
        if ($this->enabled && $this->actionKind !== null) {
            $this->actionGen = $gen;
        }
    }

    public function applyReceived(string $re, string $im, string $depth): void
    {
        if ($this->enabled) {
            $this->message(sprintf('apply received re=%s im=%s depth=%s', $re, $im, $depth));
        }
    }

    public function applyReference(int $refVersion): void
    {
        if (!$this->enabled || $this->actionKind !== 'apply') {
            return;
        }

        $this->message(sprintf(
            'apply reference %.1f ms version=%d',
            $this->actionElapsedMs(),
            $refVersion,
        ));
    }

    public function applyDispatch(int $bands): void
    {
        if (!$this->enabled || $this->actionKind !== 'apply') {
            return;
        }

        $this->message(sprintf(
            'apply dispatch %d bands %.1f ms',
            $bands,
            $this->actionElapsedMs(),
        ));
    }

    public function firstResult(int $gen): void
    {
        if (
            !$this->enabled
            || $this->actionFirstResult
            || $this->actionKind === null
            || $gen !== $this->actionGen
        ) {
            return;
        }

        $this->actionFirstResult = true;
        $this->message(sprintf(
            '%s first result %.1f ms',
            $this->actionKind,
            $this->actionElapsedMs(),
        ));
    }

    public function tick(
        int $tickCount,
        float $start,
        float $pumpEnd,
        float $eventsEnd,
        float $resultsEnd,
        float $sleepStart,
        float $sleepEnd,
    ): void {
        if (!$this->enabled) {
            return;
        }

        $total = ($sleepEnd - $start) * 1000.0;
        $pump = ($pumpEnd - $start) * 1000.0;
        $events = ($eventsEnd - $pumpEnd) * 1000.0;
        $results = ($resultsEnd - $eventsEnd) * 1000.0;
        $sleep = ($sleepEnd - $sleepStart) * 1000.0;

        $this->heartbeatPump += $pump;
        $this->heartbeatEvents += $events;
        $this->heartbeatResults += $results;
        $this->heartbeatSleep += $sleep;

        if ($total > 50.0 || $pump > 50.0 || $events > 50.0 || $results > 50.0) {
            $this->message(sprintf(
                'tick total=%.1f ms pump=%.1f events=%.1f results=%.1f sleep=%.1f',
                $total,
                $pump,
                $events,
                $results,
                $sleep,
            ));
        }

        $elapsed = $sleepEnd - $this->heartbeatSince;
        if ($elapsed >= 0.5) {
            $this->message(sprintf(
                'alive ticks=%d since=%.2fs pump=%.1f events=%.1f results=%.1f sleep=%.1f',
                $tickCount - $this->heartbeatTicks,
                $elapsed,
                $this->heartbeatPump,
                $this->heartbeatEvents,
                $this->heartbeatResults,
                $this->heartbeatSleep,
            ));

            $this->heartbeatTicks = $tickCount;
            $this->heartbeatSince = $sleepEnd;
            $this->heartbeatPump = 0.0;
            $this->heartbeatEvents = 0.0;
            $this->heartbeatResults = 0.0;
            $this->heartbeatSleep = 0.0;
        }
    }

    private function actionElapsedMs(): float
    {
        return (microtime(true) - $this->actionClickTime) * 1000.0;
    }
}
