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

(static function (): void {
    $workspace = dirname(__DIR__);
    foreach (["{$workspace}/vendor/autoload.php", "{$workspace}/../../autoload.php"] as $file) {
        if (file_exists($file)) {
            require $file;
            return;
        }
    }
    $composerJson = __DIR__ . '/../composer.json';
    if (!file_exists($composerJson)) {
        return;
    }
    $composerJson = file_get_contents($composerJson);
    $composerJson = json_decode($composerJson, true);
    $autoload = $composerJson['autoload'];
    $autoloadPsr4 = $autoload['psr-4'] ?? [];
    if ($autoloadPsr4) {
        spl_autoload_register(static function (string $class) use ($autoloadPsr4): void {
            foreach ($autoloadPsr4 as $namespace => $path) {
                if (!str_starts_with($class, $namespace)) {
                    continue;
                }
                $file = str_replace('\\', '/', substr($class, strlen($namespace))) . '.php';
                $file = "{$path}{$file}";
                if (file_exists($file)) {
                    require $file;
                    return;
                }
            }
        });
    }
    $autoloadFiles = $autoload['files'] ?? [];
    foreach ($autoloadFiles as $file) {
        require "{$workspace}/{$file}";
    }
})();
