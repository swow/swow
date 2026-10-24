<?php

declare(strict_types=1);

namespace Swow\Mandelbrot;

use InvalidArgumentException;

/**
 * Window onto the complex plane.
 *
 * The centre and the depth are canonical plain decimal text held at unlimited
 * precision. setCenter() and setDepth() expand exponent notation and drop
 * insignificant zeros, zoom() and pan() update the text with decimal
 * arithmetic, and format() returns it unchanged, so a value survives a GUI
 * round trip with every digit.
 *
 * depthText() is the exact value the GUI edits. depthFloat() is its double
 * projection, used for scale and limit here and for the direct path threshold
 * in the frame loop.
 */
final class View
{
    public const DEFAULT_CENTER_RE = '-0.743643887018689113498922249712800000007037228233600378464531';
    public const DEFAULT_CENTER_IM = '0.131825904211894657975878336475200000004669779776843260798656';
    public const DEFAULT_DEPTH = '10000';

    /** Pixels per unit of the reset view at a height of 1080 pixels. */
    public const BASE_SCALE = 300.0;
    public const BASE_HEIGHT = 1080;

    private const MIN_DEPTH = 1.0e-3;
    private const MAX_DEPTH = 1.0e300;
    private const MAX_COORDINATE = 16.0;

    private const DECIMAL = '/^[+-]?(?:\d+(?:\.\d*)?|\.\d+)(?:[eE][+-]?\d+)?$/D';

    private int $width;
    private int $height;
    private int $baseIterations;
    private string $centerRe = self::DEFAULT_CENTER_RE;
    private string $centerIm = self::DEFAULT_CENTER_IM;
    private string $depthText = self::DEFAULT_DEPTH;

    public function __construct(int $width, int $height, int $baseIterations = 1024)
    {
        if ($width < 1 || $height < 1) {
            throw new InvalidArgumentException("image size must be positive, got {$width}x{$height}");
        }
        if ($baseIterations < 1) {
            throw new InvalidArgumentException("base iterations must be positive, got {$baseIterations}");
        }
        $this->width = $width;
        $this->height = $height;
        $this->baseIterations = $baseIterations;
    }

    public function reset(): void
    {
        $this->centerRe = self::DEFAULT_CENTER_RE;
        $this->centerIm = self::DEFAULT_CENTER_IM;
        $this->depthText = self::DEFAULT_DEPTH;
    }

    public function centerRe(): string
    {
        return $this->centerRe;
    }

    public function centerIm(): string
    {
        return $this->centerIm;
    }

    /** Depth as a double, for the places that need one. */
    public function depth(): float
    {
        return $this->depthFloat();
    }

    public function depthFloat(): float
    {
        return (float) $this->depthText;
    }

    /** Exact decimal depth text, the value the GUI shows and edits. */
    public function depthText(): string
    {
        return $this->depthText;
    }

    public function width(): int
    {
        return $this->width;
    }

    public function height(): int
    {
        return $this->height;
    }

    public function scale(): float
    {
        return self::BASE_SCALE * ($this->height / self::BASE_HEIGHT) * $this->depthFloat();
    }

    public function limit(): int
    {
        $depth = $this->depthFloat();
        if ($depth <= 1.0) {
            return $this->baseIterations;
        }

        return $this->baseIterations + (int) (64.0 * (log($depth) / log(2.0)));
    }

    /**
     * Number of decimal places shown for a fresh centre.
     */
    public function digits(): int
    {
        return $this->depthDigits() + 3;
    }

    /**
     * Number of decimal places the reference orbit needs. This is the depth
     * digit count plus 20, which leaves room for the rounding the orbit
     * accumulates over the iteration limit.
     */
    public function precision(): int
    {
        return $this->depthDigits() + 20;
    }

    public function zoom(float $factor): void
    {
        if (!is_finite($factor) || $factor <= 0.0) {
            $described = is_finite($factor) ? (string) $factor : 'non-finite';
            throw new InvalidArgumentException("zoom factor must be positive and finite, got {$described}");
        }
        // Exact decimal multiply by the factor text, e.g. 2 or 0.5, so zoom in
        // and zoom out are inverse and the text does not drift
        $factorText = self::canonicalDecimal((string) $factor);
        $scale = $this->decimalPlaces($this->depthText) + $this->decimalPlaces($factorText);
        $next = self::canonicalDecimal(bcmul($this->depthText, $factorText, $scale));
        $value = (float) $next;
        if (!is_finite($value) || $value < self::MIN_DEPTH) {
            $next = self::canonicalDecimal(sprintf('%.3F', self::MIN_DEPTH));
        } elseif ($value > self::MAX_DEPTH) {
            $next = self::canonicalDecimal(sprintf('%.0F', self::MAX_DEPTH));
        }
        $this->depthText = $next;
    }

    /**
     * Moves the centre by screen pixels, x to the right and y downwards
     * positive. One pixel is 1 / scale in complex units, with the same scale
     * the renderer uses, so a pan of N pixels moves the image by N pixels.
     * The arithmetic is decimal: dc = pixels * 18 / (5 * height * depth).
     */
    public function pan(float $dxPixels, float $dyPixels, int $imageWidth, int $imageHeight): void
    {
        if ($imageWidth < 1 || $imageHeight < 1) {
            throw new InvalidArgumentException("invalid image parameters: {$imageWidth}x{$imageHeight}");
        }
        if (!is_finite($dxPixels) || !is_finite($dyPixels)) {
            throw new InvalidArgumentException('pan offset must be finite');
        }
        if ($dxPixels == 0.0 && $dyPixels == 0.0) {
            return;
        }
        // scale = 300 * (height / 1080) * depth = (5 * height / 18) * depth
        // pixels per unit, so one pixel is 18 / (5 * height * depth) units
        $scale = max(
            $this->digits() + 10,
            $this->decimalPlaces($this->centerRe),
            $this->decimalPlaces($this->centerIm)
        );
        $denominator = bcmul(bcmul('5', (string) $imageHeight, 0), $this->depthText, $scale);
        $dcRe = bcdiv(bcmul(self::canonicalDecimal((string) $dxPixels), '18', $scale), $denominator, $scale);
        $dcIm = bcdiv(bcmul(self::canonicalDecimal((string) $dyPixels), '18', $scale), $denominator, $scale);
        $this->centerRe = self::canonicalDecimal(bcsub($this->centerRe, $dcRe, $scale));
        // screen y grows downwards, so a downward pan lowers the imaginary part
        $this->centerIm = self::canonicalDecimal(bcsub($this->centerIm, $dcIm, $scale));
    }

    public function setCenter(string $re, string $im): void
    {
        $this->centerRe = $this->parseCoordinate($re, 'real');
        $this->centerIm = $this->parseCoordinate($im, 'imaginary');
    }

    public function setDepth(string $depth): void
    {
        $text = trim($depth);
        if (preg_match(self::DECIMAL, $text) !== 1) {
            throw new InvalidArgumentException("depth is not a decimal number: {$depth}");
        }
        $canonical = self::canonicalDecimal($text);
        $value = (float) $canonical;
        if (!is_finite($value) || $value < self::MIN_DEPTH || $value > self::MAX_DEPTH) {
            throw new InvalidArgumentException("depth is out of range: {$depth}");
        }
        $this->depthText = $canonical;
    }

    /**
     * @return array{re: string, im: string, depth: string}
     */
    public function format(): array
    {
        // The fields are read back by the GUI, so they carry exactly what is
        // stored: no truncation to a depth dependent width
        return [
            're' => $this->centerRe,
            'im' => $this->centerIm,
            'depth' => $this->depthText,
        ];
    }

    /**
     * @return array{centerRe: string, centerIm: string, scale: float, limit: int,
     *     fullWidth: int, fullHeight: int, width: int, height: int, step: int}
     */
    public function params(int $imageWidth, int $imageHeight, int $step): array
    {
        if ($imageWidth < 1 || $imageHeight < 1 || $step < 1) {
            throw new InvalidArgumentException("invalid image parameters: {$imageWidth}x{$imageHeight} step {$step}");
        }

        return [
            'centerRe' => $this->centerRe,
            'centerIm' => $this->centerIm,
            'scale' => $this->scale(),
            'limit' => $this->limit(),
            'fullWidth' => $imageWidth,
            'fullHeight' => $imageHeight,
            'width' => intdiv($imageWidth, $step),
            'height' => intdiv($imageHeight, $step),
            'step' => $step,
        ];
    }

    private function depthDigits(): int
    {
        $depth = $this->depthFloat();
        if ($depth < 1.0) {
            return 1;
        }
        $exponent = (int) floor(log10($depth));
        // log10 may land just below an exact power of ten
        if (10.0 ** ($exponent + 1) <= $depth) {
            $exponent++;
        } elseif (10.0 ** $exponent > $depth) {
            $exponent--;
        }

        return $exponent + 1;
    }

    private function parseCoordinate(string $value, string $name): string
    {
        $text = trim($value);
        if (preg_match(self::DECIMAL, $text) !== 1) {
            throw new InvalidArgumentException("{$name} centre is not a decimal number: {$value}");
        }
        $number = (float) $text;
        if (!is_finite($number) || abs($number) > self::MAX_COORDINATE) {
            throw new InvalidArgumentException("{$name} centre is out of range: {$value}");
        }

        return self::canonicalDecimal($text);
    }

    private function decimalPlaces(string $value): int
    {
        $dot = strpos($value, '.');
        if ($dot === false) {
            return 0;
        }

        return strlen($value) - $dot - 1;
    }

    /**
     * Canonical plain decimal text: exponent notation expanded, no leading plus,
     * no leading integer zeros, no trailing fractional zeros, at least one digit.
     * Sign is kept unless the value is zero.
     */
    private static function canonicalDecimal(string $text): string
    {
        $plain = self::expandExponent($text);
        $negative = str_starts_with($plain, '-');
        if ($negative || str_starts_with($plain, '+')) {
            $plain = substr($plain, 1);
        }
        if (str_contains($plain, '.')) {
            [$integer, $fraction] = explode('.', $plain, 2);
        } else {
            [$integer, $fraction] = [$plain, ''];
        }
        $integer = ltrim($integer, '0');
        if ($integer === '') {
            $integer = '0';
        }
        $fraction = rtrim($fraction, '0');
        $canonical = $fraction === '' ? $integer : $integer . '.' . $fraction;
        if ($negative && $canonical !== '0') {
            $canonical = '-' . $canonical;
        }

        return $canonical;
    }

    /**
     * Turns exponent notation into plain decimal text, which bcmath parses.
     */
    private static function expandExponent(string $text): string
    {
        if (stripos($text, 'e') === false) {
            return $text;
        }
        [$mantissa, $exponent] = preg_split('/[eE]/', $text, 2);
        $exponent = (int) $exponent;
        $negative = str_starts_with($mantissa, '-');
        if ($negative || str_starts_with($mantissa, '+')) {
            $mantissa = substr($mantissa, 1);
        }
        if (str_contains($mantissa, '.')) {
            [$integer, $fraction] = explode('.', $mantissa, 2);
        } else {
            [$integer, $fraction] = [$mantissa, ''];
        }
        $digits = $integer . $fraction;
        $point = strlen($integer) + $exponent;
        if ($point <= 0) {
            $plain = '0.' . str_repeat('0', -$point) . $digits;
        } elseif ($point >= strlen($digits)) {
            $plain = $digits . str_repeat('0', $point - strlen($digits));
        } else {
            $plain = substr($digits, 0, $point) . '.' . substr($digits, $point);
        }

        return ($negative ? '-' : '') . $plain;
    }
}
