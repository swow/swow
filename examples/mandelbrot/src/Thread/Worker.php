<?php

declare(strict_types=1);

namespace Swow\Mandelbrot\Thread;

use RuntimeException;
use Swow\Futex;
use Swow\Mandelbrot\Buffer;
use Swow\Mandelbrot\Reference;
use Swow\Mandelbrot\Renderer;
use Swow\Mandelbrot\View;
use Swow\SpscQueue;
use Swow\SpscQueueException;
use Swow\Thread;
use Throwable;

use function count;
use function sprintf;

/**
 * Worker thread body and spawn helper.
 *
 * Jobs arrive on a per-worker SpscQueue. Pixels and the glitch record go into
 * the shared Buffer; the small completion token that follows provides the
 * happens-before edge for the main thread.
 */
final class Worker
{
    private const RESULT_PUSH_TIMEOUT_MS = 1000;

    /** A band with more glitches than this is not repaired; the main thread warns. */
    private const REPAIR_LIMIT = 256;

    public static function spawn(Buffer $buffer, SpscQueue $tasks, SpscQueue $results, Futex $generation): Thread
    {
        $code = 'require ' . var_export(__DIR__ . '/../bootstrap.php', true) . ';'
            . self::attachCode('$buffer', serialize($buffer))
            . self::attachCode('$tasks', serialize($tasks))
            . self::attachCode('$results', serialize($results))
            . self::attachCode('$generation', serialize($generation))
            . '\Swow\Mandelbrot\Thread\Worker::loop($buffer, $tasks, $results, $generation);';

        return Thread::runCode($code);
    }

    public static function spawnService(
        Buffer $buffer,
        SpscQueue $jobs,
        SpscQueue $results,
        Futex $generation,
    ): Thread {
        return ServiceThread::spawn($buffer, $jobs, $results, $generation);
    }

    public static function loop(Buffer $buffer, SpscQueue $tasks, SpscQueue $results, Futex $generation): void
    {
        $orbit = null;
        $orbitVersion = -1;
        $reference = null;
        $referenceVersion = -1;

        while (true) {
            try {
                $task = $tasks->pop(50);
            } catch (SpscQueueException) {
                continue;
            }

            if (isset($task['quit'])) {
                return;
            }

            $gen = $task['gen'];
            $bandIndex = intdiv($task['rowBegin'], Buffer::BAND_ROWS);
            if ($generation->getValue() !== $gen) {
                self::pushToken($results, $task, $bandIndex, 0, false);
                continue;
            }

            if ($orbit === null || $orbitVersion !== $task['refVersion']) {
                $orbit = $buffer->readOrbit();
                $orbitVersion = $task['refVersion'];
            }

            $rendered = Renderer::renderBand(
                $orbit,
                $task['params'],
                $task['rowBegin'],
                $task['rowEnd'],
            );

            $width = $task['params']['width'];
            $expected = ($task['rowEnd'] - $task['rowBegin']) * $width;
            if (count($rendered['colors']) !== $expected) {
                throw new RuntimeException(sprintf(
                    'renderBand returned %d colors, expected %d',
                    count($rendered['colors']),
                    $expected,
                ));
            }

            // Drop the band when the view changed while it was rendering.
            if ($generation->getValue() !== $gen) {
                self::pushToken($results, $task, $bandIndex, 0, false);
                continue;
            }

            $buffer->writeBand($task['preview'], $task['rowBegin'], $rendered['colors']);

            $glitches = $rendered['glitches'];
            $glitchCount = intdiv(count($glitches), 2);
            $buffer->writeGlitches($bandIndex, $glitches);
            $skipRepair = $glitchCount > self::REPAIR_LIMIT;

            if ($glitchCount > 0 && !$skipRepair) {
                if ($reference === null || $referenceVersion !== $task['refVersion']) {
                    $reference = self::makeReference($task);
                    $referenceVersion = $task['refVersion'];
                }
                self::repairGlitches(
                    $buffer,
                    $reference,
                    $task['preview'],
                    $task['rowBegin'],
                    $task['rowEnd'],
                    $task['params']['step'],
                    $task['params']['limit'],
                    $glitches,
                );
            }

            self::pushToken($results, $task, $bandIndex, $glitchCount, $skipRepair);
        }
    }

    /**
     * @param array<string, mixed> $task
     */
    private static function makeReference(array $task): Reference
    {
        $view = new View(
            (int) $task['params']['fullWidth'],
            (int) $task['params']['fullHeight'],
            (int) $task['view']['base'],
        );
        $view->setCenter((string) $task['view']['re'], (string) $task['view']['im']);
        $view->setDepth((string) $task['view']['depth']);

        return new Reference($view);
    }

    /**
     * @param list<int> $glitches flat [x, y, ...] in image coordinates
     */
    public static function repairGlitches(
        Buffer $buffer,
        Reference $reference,
        bool $preview,
        int $rowBegin,
        int $rowEnd,
        int $step,
        int $limit,
        array $glitches,
    ): void {
        for ($i = 0, $count = count($glitches); $i + 1 < $count; $i += 2) {
            $x = $glitches[$i];
            $y = $glitches[$i + 1];
            if ($y < $rowBegin || $y >= $rowEnd) {
                continue;
            }

            try {
                $color = $reference->renderPixelSlow($x, $y, $step, $limit);
                $buffer->writePixel($preview, $x, $y, $color);
            } catch (Throwable) {
                continue;
            }
        }
    }

    /**
     * @param array<string, mixed> $task
     */
    private static function pushToken(
        SpscQueue $results,
        array $task,
        int $bandIndex,
        int $glitchCount,
        bool $overflow,
    ): void {
        $results->push([
            'gen' => $task['gen'],
            'bandIndex' => $bandIndex,
            'rowBegin' => $task['rowBegin'],
            'rowEnd' => $task['rowEnd'],
            'preview' => $task['preview'],
            'glitchCount' => $glitchCount,
            'overflow' => $overflow,
        ], self::RESULT_PUSH_TIMEOUT_MS);
    }

    private static function attachCode(string $variable, string $serialized): string
    {
        return $variable . ' = unserialize(base64_decode('
            . var_export(base64_encode($serialized), true) . '));';
    }
}
