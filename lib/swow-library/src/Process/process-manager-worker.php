<?php

/**
 * This file is part of Swow
 *
 * @link    https://github.com/swow/swow
 * @contact twosee <twosee@php.net>
 *
 * For the full copyright and license information,
 * please view the LICENSE file that was distributed with this source code
 */

declare(strict_types=1);

use Swow\Process\WorkerContext;

if (!extension_loaded('swow')) {
    fwrite(STDERR, "process-manager-worker: swow extension is not loaded\n");
    exit(1);
}

if (!filter_var(ini_get('swow.closure_serializer'), FILTER_VALIDATE_BOOLEAN)) {
    fwrite(STDERR, "process-manager-worker: swow.closure_serializer is not enabled\n");
    exit(1);
}

$autoload = getenv('SWOW_PROCESS_MANAGER_AUTOLOAD');
if (is_string($autoload) && $autoload !== '' && is_file($autoload)) {
    require $autoload;
} else {
    require __DIR__ . '/WorkerContext.php';
}

$payloadFd = getenv('SWOW_PROCESS_MANAGER_PAYLOAD_FD');
if (!is_string($payloadFd) || $payloadFd === '') {
    fwrite(STDERR, "process-manager-worker: payload fd is missing\n");
    exit(1);
}

$payloadStream = fopen("php://fd/{$payloadFd}", 'rb');
if ($payloadStream === false) {
    fwrite(STDERR, "process-manager-worker: unable to open payload fd\n");
    exit(1);
}
$payload = stream_get_contents($payloadStream);
fclose($payloadStream);

try {
    $closure = unserialize($payload, ['allowed_classes' => true]);
} catch (Throwable $e) {
    fwrite(STDERR, "process-manager-worker: unable to unserialize closure: {$e->getMessage()}\n");
    exit(1);
}

if (!$closure instanceof Closure) {
    fwrite(STDERR, "process-manager-worker: payload is not a closure\n");
    exit(1);
}

$context = new WorkerContext(
    (int) getenv('SWOW_PROCESS_MANAGER_WORKER_ID'),
    getmypid(),
    (int) getenv('SWOW_PROCESS_MANAGER_MANAGER_PID'),
    (int) getenv('SWOW_PROCESS_MANAGER_WORKER_COUNT'),
);

try {
    $closure($context);
} catch (Throwable $e) {
    fwrite(STDERR, sprintf(
        "process-manager-worker: uncaught %s: %s in %s:%d\n",
        $e::class,
        $e->getMessage(),
        $e->getFile(),
        $e->getLine()
    ));
    exit(1);
}

exit(0);
