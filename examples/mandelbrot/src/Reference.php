<?php

declare(strict_types=1);

namespace Swow\Mandelbrot;

/**
 * Reference orbit of the view centre.
 *
 * Z(n+1) = Z(n)^2 + c0 is iterated with bcmath at the precision the depth
 * needs, with index n holding Z(n) and index 0 at zero. The double projections
 * of the orbit feed the worker delta iteration. The build stops at the
 * iteration limit or at the first value past the escape bound.
 *
 * The build also keeps the CANDIDATE_COUNT smallest |Z| values with their full
 * precision decimal text, returned by highPrecision().
 *
 * renderPixelSlow() iterates one pixel in bcmath from the view centre, for
 * pixels the double path cannot resolve. Its escape count and radius match the
 * direct double path, so a slow pixel agrees with a fast one wherever both are
 * accurate.
 */
final class Reference
{
    /** Number of low-|Z| samples kept in high precision. */
    public const CANDIDATE_COUNT = 64;

    /** A reference orbit that passes this |Z|^2 has escaped for every purpose. */
    private const DIVERGED = 1.0e100;

    private View $view;
    private int $precision;
    private int $version = 0;
    private int $limit = 0;
    private string $cRe = '0';
    private string $cIm = '0';

    /** @var array{zr: list<float>, zi: list<float>, mag: list<float>} */
    private array $orbit = ['zr' => [], 'zi' => [], 'mag' => []];

    /** @var list<array{index: int, re: string, im: string, mag: float}> */
    private array $candidates = [];

    public function __construct(View $view)
    {
        $this->view = $view;
        $this->precision = $view->precision();
        $this->limit = $view->limit();
    }

    /**
     * Rebuilds the orbit from the current view state and returns the new
     * reference version.
     */
    public function build(): int
    {
        $this->precision = $this->view->precision();
        $this->limit = $this->view->limit();
        $precision = $this->precision;
        $limit = $this->limit;
        $this->cRe = bcadd($this->view->centerRe(), '0', $precision);
        $this->cIm = bcadd($this->view->centerIm(), '0', $precision);

        $zrList = [];
        $ziList = [];
        $magList = [];
        $candidates = [];
        $worst = INF;
        $sortCandidates = static function (array &$list): void {
            usort($list, static fn (array $a, array $b): int => $a['mag'] <=> $b['mag']);
        };

        $zr = '0';
        $zi = '0';
        $zr2 = '0';
        $zi2 = '0';
        $divergedAt = -1;

        for ($n = 0; $n <= $limit; $n++) {
            $zrList[$n] = (float) $zr;
            $ziList[$n] = (float) $zi;
            $mag = (float) bcadd($zr2, $zi2, $precision);
            $magList[$n] = $mag;

            if (count($candidates) < self::CANDIDATE_COUNT) {
                $candidates[] = ['index' => $n, 're' => $zr, 'im' => $zi, 'mag' => $mag];
                if (count($candidates) === self::CANDIDATE_COUNT) {
                    $sortCandidates($candidates);
                    $worst = $candidates[self::CANDIDATE_COUNT - 1]['mag'];
                }
            } elseif ($mag < $worst) {
                $candidates[self::CANDIDATE_COUNT - 1] = ['index' => $n, 're' => $zr, 'im' => $zi, 'mag' => $mag];
                $sortCandidates($candidates);
                $worst = $candidates[self::CANDIDATE_COUNT - 1]['mag'];
            }

            if ($mag > self::DIVERGED || !is_finite($mag)) {
                $divergedAt = $n;
                break;
            }

            $twiceZr = bcadd($zr, $zr, $precision);
            $nextZi = bcadd(bcmul($twiceZr, $zi, $precision), $this->cIm, $precision);
            $nextZr = bcadd(bcsub($zr2, $zi2, $precision), $this->cRe, $precision);
            $zr = $nextZr;
            $zi = $nextZi;
            $zr2 = bcmul($zr, $zr, $precision);
            $zi2 = bcmul($zi, $zi, $precision);
        }

        if ($divergedAt >= 0) {
            // keep the array the length the buffer expects; the tail is not usable
            for ($n = $divergedAt + 1; $n <= $limit; $n++) {
                $zrList[$n] = 0.0;
                $ziList[$n] = 0.0;
                $magList[$n] = INF;
            }
        }

        $this->orbit = ['zr' => $zrList, 'zi' => $ziList, 'mag' => $magList];
        $this->candidates = $candidates;

        return ++$this->version;
    }

    /**
     * @return array{zr: list<float>, zi: list<float>, mag: list<float>}
     */
    public function orbit(): array
    {
        return $this->orbit;
    }

    /**
     * The entries with the smallest |Z|, sorted by |Z|, with the full precision
     * real and imaginary parts.
     *
     * @return list<array{index: int, re: string, im: string, mag: float}>
     */
    public function highPrecision(): array
    {
        return $this->candidates;
    }

    /**
     * Direct bcmath iteration of one pixel, for pixels the double path cannot
     * resolve. The escape count and radius match the direct double path so the
     * slow result agrees with a fast result wherever both are accurate.
     */
    public function renderPixelSlow(int $x, int $y, int $step, int $limit): int
    {
        $precision = $this->precision;
        $scale = $this->view->scale();
        $scaleText = $scale >= 1.0
            ? number_format($scale, 0, '.', '')
            : number_format($scale, 20, '.', '');
        $offsetX = $x * $step + $step / 2 - $this->view->width() / 2;
        // row 0 is the top of the image and the highest imaginary part
        $offsetY = $this->view->height() / 2 - ($y * $step + $step / 2);
        // the offset is an exact small numerator over the scale, so bcmath can
        // carry it below what a double division would keep
        $cRe = bcadd($this->view->centerRe(), bcdiv(sprintf('%.1f', $offsetX), $scaleText, $precision), $precision);
        $cIm = bcadd($this->view->centerIm(), bcdiv(sprintf('%.1f', $offsetY), $scaleText, $precision), $precision);

        $zr = '0';
        $zi = '0';
        for ($n = 0; $n < $limit; $n++) {
            $zr2 = bcmul($zr, $zr, $precision);
            $zi2 = bcmul($zi, $zi, $precision);
            $mag = bcadd($zr2, $zi2, $precision);
            if (bccomp($mag, '4', $precision) > 0) {
                return Palette::color($n, $limit, (float) $mag);
            }
            $nextZi = bcadd(bcmul(bcadd($zr, $zr, $precision), $zi, $precision), $cIm, $precision);
            $zr = bcadd(bcsub($zr2, $zi2, $precision), $cRe, $precision);
            $zi = $nextZi;
        }

        return Palette::INSIDE;
    }

    public function precision(): int
    {
        return $this->precision;
    }
}
