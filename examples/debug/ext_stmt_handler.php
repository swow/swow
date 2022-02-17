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

namespace Swow\Example\Debug\Debugger\ExtendedStatementHandler;

use Swow\Coroutine;
use function Swow\Debug\registerExtendedStatementHandler;

registerExtendedStatementHandler(static function (): void {
    $coroutine = Coroutine::getCurrent();
    $file = $coroutine->getExecutedFilename(2);
    $line = $coroutine->getExecutedLineno(2);
    $function = $coroutine->getExecutedFunctionName(2);
    echo "{$file}:{$line}\n{$function}()\n";
});

Coroutine::run(static function (): void {
    (static function (): void {
        echo "Hello\n";
    })();
});
