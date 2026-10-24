<?php

declare(strict_types=1);

/*
 * Runtime guard and PSR-4 style autoloader for Swow\Mandelbrot.
 *
 * Nested namespaces map to subdirectories, so App\WindowLoop resolves to
 * App/WindowLoop.php and Thread\Worker to Thread/Worker.php. Worker threads
 * and tests include this file directly, so it must not depend on anything
 * above it.
 */

$missing = [];
foreach (['swow', 'bcmath', 'zlib'] as $extension) {
    if (!extension_loaded($extension)) {
        $missing[] = $extension;
    }
}

if ($missing !== []) {
    throw new RuntimeException('missing required PHP extension(s): ' . implode(', ', $missing));
}

if (!PHP_ZTS) {
    throw new RuntimeException('PHP must be built with ZTS (thread safety)');
}

spl_autoload_register(static function (string $class): void {
    $prefix = 'Swow\Mandelbrot\\';
    if (!str_starts_with($class, $prefix)) {
        return;
    }

    $file = __DIR__ . '/' . str_replace('\\', '/', substr($class, strlen($prefix))) . '.php';
    if (is_file($file)) {
        require $file;
    }
});
