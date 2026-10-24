<?php

declare(strict_types=1);

namespace Swow\Mandelbrot;

use InvalidArgumentException;

final class Config
{
    private const DEFAULT_WIDTH = 1280;
    private const DEFAULT_HEIGHT = 720;
    private const DEFAULT_CENTER_RE = '-0.743643887018689113498922249712800000007037228233600378464531';
    private const DEFAULT_CENTER_IM = '0.131825904211894657975878336475200000004669779776843260798656';
    private const DEFAULT_DEPTH = '1e4';
    private const DEFAULT_ITERATIONS = 1024;
    private const DEFAULT_OUTPUT = 'mandelbrot.png';
    private const DEFAULT_FRAMES = 1;

    private const MAX_THREADS = 256;
    private const MAX_DIMENSION = 16384;
    private const MAX_ITERATIONS = 1_000_000;
    private const MAX_FRAMES = 1_000_000;
    private const DEFAULT_AUTO_MS = 200;
    private const MAX_AUTO_MS = PHP_INT_MAX;

    private const DECIMAL_PATTERN = '/^[+-]?(?:\d+(?:\.\d*)?|\.\d+)(?:[eE][+-]?\d+)?$/';

    private function __construct(
        private readonly bool $headless,
        private readonly int $threads,
        private readonly int $width,
        private readonly int $height,
        private readonly string $centerRe,
        private readonly string $centerIm,
        private readonly string $depthText,
        private readonly int $baseIterations,
        private readonly string $output,
        private readonly int $frames,
        private readonly bool $verify,
        private readonly bool $auto,
        private readonly int $autoMs,
    ) {
    }

    /**
     * @param list<string> $argv
     *
     * @throws InvalidArgumentException on an unknown option or an invalid value
     */
    public static function fromArgv(array $argv): self
    {
        $headless = false;
        $threads = self::defaultThreads();
        $width = self::DEFAULT_WIDTH;
        $height = self::DEFAULT_HEIGHT;
        $centerRe = self::DEFAULT_CENTER_RE;
        $centerIm = self::DEFAULT_CENTER_IM;
        $depthText = self::DEFAULT_DEPTH;
        $baseIterations = self::DEFAULT_ITERATIONS;
        $output = self::DEFAULT_OUTPUT;
        $frames = self::DEFAULT_FRAMES;
        $verify = false;
        $auto = false;
        $autoMs = self::DEFAULT_AUTO_MS;

        $count = count($argv);
        for ($i = 1; $i < $count; $i++) {
            $arg = $argv[$i];
            if (!str_starts_with($arg, '--')) {
                throw new InvalidArgumentException("unexpected argument: {$arg}");
            }

            $separator = strpos($arg, '=');
            if ($separator === false) {
                $name = $arg;
                $inline = null;
            } else {
                $name = substr($arg, 0, $separator);
                $inline = substr($arg, $separator + 1);
            }

            switch ($name) {
                case '--headless':
                    self::rejectValue($name, $inline);
                    $headless = true;
                    break;
                case '--threads':
                    $threads = self::parseInt($name, self::takeValue($argv, $i, $name, $inline, $count), 1, self::MAX_THREADS);
                    break;
                case '--width':
                    $width = self::parseInt($name, self::takeValue($argv, $i, $name, $inline, $count), 4, self::MAX_DIMENSION);
                    break;
                case '--height':
                    $height = self::parseInt($name, self::takeValue($argv, $i, $name, $inline, $count), 4, self::MAX_DIMENSION);
                    break;
                case '--center':
                    [$centerRe, $centerIm] = self::parseCenter(self::takeValue($argv, $i, $name, $inline, $count));
                    break;
                case '--depth':
                    $depthText = self::parseDepth(self::takeValue($argv, $i, $name, $inline, $count));
                    break;
                case '--iterations':
                    $baseIterations = self::parseInt($name, self::takeValue($argv, $i, $name, $inline, $count), 1, self::MAX_ITERATIONS);
                    break;
                case '--output':
                    $output = self::takeValue($argv, $i, $name, $inline, $count);
                    if ($output === '') {
                        throw new InvalidArgumentException('--output must not be empty');
                    }
                    break;
                case '--frames':
                    $frames = self::parseInt($name, self::takeValue($argv, $i, $name, $inline, $count), 1, self::MAX_FRAMES);
                    break;
                case '--verify':
                    self::rejectValue($name, $inline);
                    $verify = true;
                    break;
                case '--auto':
                    $auto = true;
                    if ($inline !== null) {
                        $autoMs = self::parseInt($name, $inline, 1, self::MAX_AUTO_MS);
                    }
                    break;
                default:
                    throw new InvalidArgumentException("unknown option: {$name}");
            }
        }

        if ($auto && $headless) {
            throw new InvalidArgumentException('--auto needs a window');
        }

        return new self(
            $headless,
            $threads,
            $width,
            $height,
            $centerRe,
            $centerIm,
            $depthText,
            $baseIterations,
            $output,
            $frames,
            $verify,
            $auto,
            $autoMs,
        );
    }

    public static function usage(): string
    {
        $center = self::DEFAULT_CENTER_RE . ',' . self::DEFAULT_CENTER_IM;
        $text = <<<'TEXT'
            Usage: mandelbrot [options]

            Options:
              --headless        render without the Tk window and write PNG only
              --threads=N       worker thread count (default: CPU count - 1)
              --width=N         image width in pixels (default: 1280)
              --height=N        image height in pixels (default: 720)
              --center=RE,IM    view center as decimal numbers
                                (default: __CENTER__)
              --depth=N         zoom depth relative to the default view (default: 1e4)
              --iterations=N    base iteration limit (default: 1024)
              --output=PATH     output PNG path (default: mandelbrot.png)
              --frames=N        number of frames to render (default: 1)
              --verify          compare thread counts and sample the slow path
              --auto[=MS]       zoom in every MS milliseconds when a frame completes
                                (default 200; window only)
              --help            show this help

            TEXT;

        return str_replace('__CENTER__', $center, $text);
    }

    public function headless(): bool
    {
        return $this->headless;
    }

    public function threads(): int
    {
        return $this->threads;
    }

    public function width(): int
    {
        return $this->width;
    }

    public function height(): int
    {
        return $this->height;
    }

    public function centerRe(): string
    {
        return $this->centerRe;
    }

    public function centerIm(): string
    {
        return $this->centerIm;
    }

    public function depth(): float
    {
        return (float) $this->depthText;
    }

    public function depthText(): string
    {
        return $this->depthText;
    }

    public function baseIterations(): int
    {
        return $this->baseIterations;
    }

    public function output(): string
    {
        return $this->output;
    }

    public function frames(): int
    {
        return $this->frames;
    }

    public function verify(): bool
    {
        return $this->verify;
    }

    public function auto(): bool
    {
        return $this->auto;
    }

    public function autoMs(): int
    {
        return $this->autoMs;
    }

    private static function defaultThreads(): int
    {
        $path = '/proc/cpuinfo';
        $info = is_readable($path) ? file_get_contents($path) : false;
        if ($info === false) {
            return 1;
        }

        $count = preg_match_all('/^processor\s*:/m', $info);
        if ($count === false || $count === 0) {
            return 1;
        }

        return max(1, $count - 1);
    }

    /**
     * @param list<string> $argv
     */
    private static function takeValue(array $argv, int &$index, string $name, ?string $inline, int $count): string
    {
        if ($inline !== null) {
            return $inline;
        }

        $index++;
        if ($index >= $count) {
            throw new InvalidArgumentException("option {$name} requires a value");
        }

        return $argv[$index];
    }

    private static function rejectValue(string $name, ?string $inline): void
    {
        if ($inline !== null) {
            throw new InvalidArgumentException("option {$name} does not take a value");
        }
    }

    private static function parseInt(string $name, string $raw, int $min, int $max): int
    {
        if (preg_match('/^\d+$/', $raw) !== 1) {
            throw new InvalidArgumentException("option {$name} requires an integer: {$raw}");
        }

        $value = (int) $raw;
        if ($value < $min || $value > $max) {
            throw new InvalidArgumentException("option {$name} must be between {$min} and {$max}: {$raw}");
        }

        return $value;
    }

    /**
     * @return array{string, string}
     */
    private static function parseCenter(string $raw): array
    {
        $parts = explode(',', $raw);
        if (count($parts) !== 2) {
            throw new InvalidArgumentException("option --center requires RE,IM: {$raw}");
        }

        $re = trim($parts[0]);
        $im = trim($parts[1]);
        foreach ([$re, $im] as $part) {
            if ($part === '' || strlen($part) > 256 || preg_match(self::DECIMAL_PATTERN, $part) !== 1) {
                throw new InvalidArgumentException("invalid decimal coordinate in --center: {$raw}");
            }
        }

        return [$re, $im];
    }

    private static function parseDepth(string $raw): string
    {
        if (strlen($raw) > 64 || preg_match(self::DECIMAL_PATTERN, $raw) !== 1) {
            throw new InvalidArgumentException("invalid decimal depth: {$raw}");
        }

        $depth = (float) $raw;
        if (!is_finite($depth) || $depth <= 0.0) {
            throw new InvalidArgumentException("--depth must be a positive finite number: {$raw}");
        }

        return $raw;
    }
}
