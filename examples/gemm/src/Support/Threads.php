<?php

declare(strict_types=1);

namespace Swow\Examples\Gemm\Support;

use RuntimeException;
use Swow\Thread;

use function array_values;
use function base64_encode;
use function dirname;
use function implode;
use function is_object;
use function serialize;
use function var_export;

/*
 * Start a worker thread with shared objects.
 *
 * Shared objects (SharedBuffer, Futex, ThreadBarrier) are passed by shared id
 * through serialize, so every worker maps the same memory. Scalars are inlined
 * as literals.
 */
final class Threads
{
    public const TIMEOUT = 300_000;

    /**
     * @param list<object|int|float|string|bool|null> $args
     */
    public static function spawn(string $callable, array $args, ?string $autoload = null): Thread
    {
        $code = 'require ' . var_export($autoload ?? dirname(__DIR__, 2) . '/autoload.php', true) . ';';
        $parts = [];
        foreach (array_values($args) as $index => $arg) {
            if (is_object($arg)) {
                $name = '$shared' . $index;
                $code .= $name . ' = unserialize(base64_decode('
                    . var_export(base64_encode(serialize($arg)), true) . '));';
                $parts[] = $name;
            } else {
                $parts[] = var_export($arg, true);
            }
        }
        $code .= '\\' . $callable . '(' . implode(', ', $parts) . ');';

        return Thread::runCode($code);
    }

    /** @param list<Thread> $workers */
    public static function join(array $workers): void
    {
        foreach ($workers as $id => $worker) {
            $worker->wait(self::TIMEOUT);
            $status = $worker->getExitStatus();
            if ($status !== 0) {
                throw new RuntimeException("Worker {$id} exited with status {$status}");
            }
        }
    }
}
