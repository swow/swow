#!/usr/bin/env php
<?php
/**
 * This file is part of Swow
 *
 * @link     https://github.com/swow/swow
 * @contact  twosee <twosee@php.net>
 *
 * For the full copyright and license information,
 * please view the LICENSE file that was distributed with this source code
 */

declare(strict_types=1);

use Rector\Core\Configuration\Option;
use Rector\Php71\Rector\FuncCall\CountOnNullRector;
use Rector\Php74\Rector\Closure\ClosureToArrowFunctionRector;
use Rector\Set\ValueObject\LevelSetList;
use Symfony\Component\DependencyInjection\Loader\Configurator\ContainerConfigurator;

if (PHP_VERSION_ID >= 80200) {
    throw new Error('Please use released PHP version');
}
if (extension_loaded(Swow::class)) {
    throw new Error(
        'Unable to run with Swow extension, ' .
        'because PHPStan\\BetterReflection does not support generate stub files for PHP-8.x extension, ' .
        'we use our stub file instead'
    );
}

return static function (ContainerConfigurator $containerConfigurator): void {
    // get parameters
    $stubFile = dirname(__DIR__) . '/lib/swow-stub/src/Swow.php';
    $parameters = $containerConfigurator->parameters();
    $parameters
        ->set(Option::CLEAR_CACHE, true)
        ->set(Option::FILE_EXTENSIONS, ['php'])
        ->set(Option::BOOTSTRAP_FILES, [
            $stubFile,
            __DIR__ . '/autoload.php',
        ])->set(Option::PATHS, [
            dirname(__DIR__) . '/benchmark',
            dirname(__DIR__) . '/examples',
            dirname(__DIR__) . '/lib',
            dirname(__DIR__) . '/tools',
        ])->set(Option::SKIP, [
            $stubFile,
        ]);
    // Define what rule sets will be applied
    $containerConfigurator->import(LevelSetList::UP_TO_PHP_80);
    // Some custom configurations
    $services = $containerConfigurator->services();
    /* I do not like this, it makes a lot of long lines of code */
    $services->remove(ClosureToArrowFunctionRector::class);
    /* It can not recognize vars which were assigned by passing to reference parameters */
    $services->remove(CountOnNullRector::class);
};
