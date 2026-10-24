<?php

declare(strict_types=1);

namespace Swow\Mandelbrot;

/**
 * Maps an iteration count to a smooth escape-time colour.
 *
 * The formula matches the libcat reference demo:
 *
 *   smooth = n + 1 - log2(log(|z|))
 *   phase  = sqrt(smooth) * 0.05
 *   colour = (cos(2*pi*phase), cos(2*pi*(phase+0.33)), cos(2*pi*(phase+0.67)))
 *
 * A colour is 0x00RRGGBB. The alpha byte is left zero because the shared
 * buffer stores BGRA and the writer supplies 255.
 */
final class Palette
{
    /** Colour of a pixel that did not escape. */
    public const INSIDE = 0x000000;

    private const TAU = 6.283185307179586;

    public static function color(int $iteration, int $limit, float $magnitudeSquared): int
    {
        if ($iteration >= $limit) {
            return self::INSIDE;
        }
        // log2(log(|z|)) needs a |z| above one, which the escape radius gives
        if (!is_finite($magnitudeSquared) || $magnitudeSquared <= 0.0) {
            return self::INSIDE;
        }
        $smooth = $iteration + 1.0 - log(log(sqrt($magnitudeSquared))) / log(2.0);
        if (!is_finite($smooth) || $smooth < 0.0) {
            return self::INSIDE;
        }

        $phase = sqrt($smooth) * 0.05;
        // truncation matches the C cast in the reference demo
        $red = (int) (255.0 * (0.5 + 0.5 * cos(self::TAU * ($phase + 0.00))));
        $green = (int) (255.0 * (0.5 + 0.5 * cos(self::TAU * ($phase + 0.33))));
        $blue = (int) (255.0 * (0.5 + 0.5 * cos(self::TAU * ($phase + 0.67))));

        return ($red << 16) | ($green << 8) | $blue;
    }
}
