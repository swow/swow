<?php

declare(strict_types=1);

namespace Swow\Mandelbrot;

use Swow\Futex;
use Swow\Mandelbrot\App\FramePipeline;
use Swow\Mandelbrot\App\HeadlessRunner;
use Swow\Mandelbrot\App\ServiceClient;
use Swow\Mandelbrot\App\Trace;
use Swow\Mandelbrot\App\WindowLoop;
use Swow\Mandelbrot\Gui\Backend;
use Throwable;

/**
 * Assembly for the headless and window front ends.
 */
final class App
{
    private readonly Trace $trace;

    public function __construct(private readonly Config $config, private ?Backend $gui)
    {
        $this->trace = new Trace();
    }

    public function run(): int
    {
        if ($this->config->headless()) {
            return $this->runHeadless();
        }

        return $this->runWindow();
    }

    private function runHeadless(): int
    {
        [$view, $buffer, $pipeline, $service] = $this->boot(
            $this->config->width(),
            $this->config->height(),
        );

        return (new HeadlessRunner($this->config, $view, $buffer, $pipeline, $service))->run();
    }

    private function runWindow(): int
    {
        if ($this->gui === null) {
            return $this->runHeadless();
        }

        try {
            $this->gui->open($this->config->width(), $this->config->height());
        } catch (Throwable $error) {
            fwrite(STDERR, 'mandelbrot: GUI unavailable, falling back to headless: '
                . $error->getMessage() . "\n");
            try {
                $this->gui->close();
            } catch (Throwable) {
            }
            $this->gui = null;

            return $this->runHeadless();
        }

        [$view, $buffer, $pipeline, $service] = $this->boot(
            $this->gui->imageWidth(),
            $this->gui->imageHeight(),
        );

        return (new WindowLoop(
            $this->config,
            $this->gui,
            $view,
            $buffer,
            $pipeline,
            $service,
            $this->trace,
        ))->run();
    }

    /**
     * @return array{View, Buffer, FramePipeline, ServiceClient}
     */
    private function boot(int $width, int $height): array
    {
        $view = new View($width, $height, $this->config->baseIterations());
        $view->setCenter($this->config->centerRe(), $this->config->centerIm());
        $view->setDepth($this->config->depthText());

        // Leave room for the iteration limit at the deepest view (64 per
        // doubling of depth) on top of the configured base limit.
        $maxLimit = max(
            Buffer::DEFAULT_MAX_LIMIT,
            $this->config->baseIterations() + 64 * 1000,
        );
        $buffer = Buffer::create($width, $height, $maxLimit);
        $generation = new Futex(0);
        $pipeline = new FramePipeline(
            $this->config,
            $this->gui,
            $view,
            $buffer,
            $generation,
            $this->trace,
        );
        $service = new ServiceClient(
            $this->config,
            $this->gui,
            $view,
            $buffer,
            $generation,
            $pipeline,
            $this->trace,
        );

        return [$view, $buffer, $pipeline, $service];
    }
}
