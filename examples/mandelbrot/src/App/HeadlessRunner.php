<?php

declare(strict_types=1);

namespace Swow\Mandelbrot\App;

use Swow\Mandelbrot\Buffer;
use Swow\Mandelbrot\Config;
use Swow\Mandelbrot\Reference;
use Swow\Mandelbrot\View;

use function count;
use function sprintf;

use const STDERR;

/**
 * --headless frame loop, including --verify.
 */
final class HeadlessRunner
{
    private const FRAME_DEPTH_FACTOR = 1.05;
    private const VERIFY_SAMPLE_COLUMNS = 8;
    private const VERIFY_SAMPLE_ROWS = 4;

    public function __construct(
        private readonly Config $config,
        private readonly View $view,
        private readonly Buffer $buffer,
        private readonly FramePipeline $pipeline,
        private readonly ServiceClient $service,
    ) {
    }

    public function run(): int
    {
        $frames = $this->config->frames();
        try {
            for ($frame = 1; $frame <= $frames; $frame++) {
                $bytes = $this->renderFrame($this->config->threads());
                $this->service->save($this->frameOutputPath($frame, $frames));

                if ($this->config->verify() && !$this->verifyFrame($bytes)) {
                    return 1;
                }

                if ($frame < $frames) {
                    $this->view->zoom(self::FRAME_DEPTH_FACTOR);
                }
            }
        } finally {
            $this->pipeline->stopWorkers();
            $this->service->stop();
        }

        return 0;
    }

    private function renderFrame(int $threads): string
    {
        if ($this->pipeline->workerCount() !== $threads) {
            $this->pipeline->stopWorkers();
            $this->pipeline->startWorkers($threads);
        }

        $this->service->requestBuild(false);
        $this->service->awaitBuild();
        $this->pipeline->awaitFrame();

        return $this->buffer->readAll(false);
    }

    private function verifyFrame(string $multiThreadBytes): bool
    {
        if ($this->config->threads() > 1) {
            $singleThreadBytes = $this->renderFrame(1);
            if ($singleThreadBytes !== $multiThreadBytes) {
                fwrite(STDERR, "verify: thread count changes the frame\n");

                return false;
            }
        }

        return $this->verifySlowSamples($multiThreadBytes);
    }

    /**
     * Compare a spread of rendered pixels against the bcmath slow path.
     */
    private function verifySlowSamples(string $bytes): bool
    {
        $reference = new Reference($this->view);
        $params = $this->pipeline->frameParams();
        $width = $params['width'];
        $height = $params['height'];
        $step = $params['step'];
        $limit = $params['limit'];
        $sampleCount = self::VERIFY_SAMPLE_COLUMNS * self::VERIFY_SAMPLE_ROWS;
        $failures = [];

        for ($row = 0; $row < self::VERIFY_SAMPLE_ROWS; $row++) {
            for ($column = 0; $column < self::VERIFY_SAMPLE_COLUMNS; $column++) {
                $x = min($width - 1, intdiv((2 * $column + 1) * $width, 2 * self::VERIFY_SAMPLE_COLUMNS));
                $y = min($height - 1, intdiv((2 * $row + 1) * $height, 2 * self::VERIFY_SAMPLE_ROWS));
                $slow = $reference->renderPixelSlow($x, $y, $step, $limit) & 0xFFFFFF;
                $actual = unpack('V', substr($bytes, ($y * $width + $x) * 4, 4))[1] & 0xFFFFFF;
                if ($actual !== $slow) {
                    $failures[] = sprintf('%d,%d buffer=0x%06X slow=0x%06X', $x, $y, $actual, $slow);
                }
            }
        }

        fprintf(
            STDERR,
            "verify: slow-path sample %d/%d matched\n",
            $sampleCount - count($failures),
            $sampleCount,
        );
        foreach ($failures as $failure) {
            fwrite(STDERR, "verify: mismatch {$failure}\n");
        }

        return $failures === [];
    }

    private function frameOutputPath(int $frame, int $frames): string
    {
        $output = $this->config->output();
        if ($frames <= 1) {
            return $output;
        }

        $info = pathinfo($output);
        $extension = isset($info['extension']) && $info['extension'] !== ''
            ? '.' . $info['extension']
            : '';

        return ($info['dirname'] ?? '.') . '/' . ($info['filename'] ?? 'mandelbrot')
            . '-' . $frame . $extension;
    }
}
