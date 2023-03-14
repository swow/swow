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

use function Swow\Debug\registerExtendedStatementHandler;

$handler = registerExtendedStatementHandler(static function (): void {
    echo "Handler\n";
});
echo "Remove Handler\n";
$handler->remove();
echo "I am gone\n";

echo "Done\n";
