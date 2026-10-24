<?php

declare(strict_types=1);

spl_autoload_register(static function (string $class): void {
    $prefix = 'Swow\Mandelbrot\\';
    if (!str_starts_with($class, $prefix)) {
        return;
    }
    $file = dirname(__DIR__, 2) . '/src/' . str_replace('\\', '/', substr($class, strlen($prefix))) . '.php';
    if (is_file($file)) {
        require $file;
    }
});
