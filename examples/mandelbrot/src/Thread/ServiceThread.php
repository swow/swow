<?php

declare(strict_types=1);

namespace Swow\Mandelbrot\Thread;

use Swow\Futex;
use Swow\SpscQueue;
use Swow\SpscQueueException;
use Swow\Thread;
use Swow\Mandelbrot\Buffer;
use Swow\Mandelbrot\Image;
use Swow\Mandelbrot\Reference;
use Swow\Mandelbrot\View;
use Throwable;

/**
 * Service thread body and spawn helper.
 *
 * The service thread owns the reference build and the PNG save, so the main
 * thread never blocks on either.
 */
final class ServiceThread
{
    private const RESULT_PUSH_TIMEOUT_MS = 1000;

    public static function spawn(
        Buffer $buffer,
        SpscQueue $jobs,
        SpscQueue $results,
        Futex $generation,
    ): Thread {
        $code = 'require ' . var_export(__DIR__ . '/../bootstrap.php', true) . ';'
            . self::attachCode('$buffer', serialize($buffer))
            . self::attachCode('$jobs', serialize($jobs))
            . self::attachCode('$results', serialize($results))
            . self::attachCode('$generation', serialize($generation))
            . '\\Swow\\Mandelbrot\\Thread\\ServiceThread::loop($buffer, $jobs, $results, $generation);';

        return Thread::runCode($code);
    }

    public static function loop(
        Buffer $buffer,
        SpscQueue $jobs,
        SpscQueue $results,
        Futex $generation,
    ): void {
        while (true) {
            try {
                $job = $jobs->pop(50);
            } catch (SpscQueueException) {
                continue;
            }

            if (isset($job['quit'])) {
                return;
            }

            try {
                if (($job['type'] ?? '') === 'build') {
                    $token = self::buildReference($buffer, $job, $generation);
                } elseif (($job['type'] ?? '') === 'save') {
                    $token = self::saveImage($buffer, $job);
                } else {
                    $token = ['type' => 'error', 'ok' => false, 'error' => 'unknown service job'];
                }
            } catch (Throwable $error) {
                $token = [
                    'type' => $job['type'] ?? 'error',
                    'gen' => $job['gen'] ?? -1,
                    'refVersion' => $job['refVersion'] ?? -1,
                    'path' => $job['path'] ?? '',
                    'ok' => false,
                    'error' => $error->getMessage(),
                ];
            }

            $results->push($token, self::RESULT_PUSH_TIMEOUT_MS);
        }
    }

    /**
     * @param array<string, mixed> $job
     * @return array<string, mixed>
     */
    private static function buildReference(Buffer $buffer, array $job, Futex $generation): array
    {
        $gen = $job['gen'];
        if ($generation->getValue() !== $gen) {
            return ['type' => 'build', 'gen' => $gen, 'refVersion' => $job['refVersion'], 'ok' => false];
        }

        $view = new View(
            (int) $job['view']['width'],
            (int) $job['view']['height'],
            (int) $job['view']['base'],
        );
        $view->setCenter((string) $job['view']['re'], (string) $job['view']['im']);
        $view->setDepth((string) $job['view']['depth']);

        $started = microtime(true);
        $reference = new Reference($view);
        $reference->build();
        $seconds = microtime(true) - $started;

        if ($generation->getValue() !== $gen) {
            return [
                'type' => 'build',
                'gen' => $gen,
                'refVersion' => $job['refVersion'],
                'ok' => false,
            ];
        }

        $buffer->writeHeader([
            'limit' => $job['limit'],
            'refVersion' => $job['refVersion'],
        ]);
        $orbit = $reference->orbit();
        $buffer->writeOrbit($orbit['zr'], $orbit['zi'], $orbit['mag']);

        return [
            'type' => 'build',
            'gen' => $gen,
            'refVersion' => $job['refVersion'],
            'ok' => true,
            'seconds' => $seconds,
        ];
    }

    /**
     * @param array<string, mixed> $job
     * @return array<string, mixed>
     */
    private static function saveImage(Buffer $buffer, array $job): array
    {
        $bytes = $buffer->readAll(false);
        Image::save($job['path'], $bytes, (int) $job['width'], (int) $job['height']);

        return [
            'type' => 'save',
            'ok' => true,
            'path' => $job['path'],
        ];
    }

    private static function attachCode(string $variable, string $serialized): string
    {
        return $variable . ' = unserialize(base64_decode('
            . var_export(base64_encode($serialized), true) . '));';
    }
}
