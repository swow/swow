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

require __DIR__ . '/StubGenerator.php';

use Swow\Extension\StubGenerator;

(function () {
    global $argv;
    if (empty($argv[1])) {
        exit('Usage: php ' . basename(__FILE__) . ' <extension_name> [output_path]');
    }
    $n = $argv[1];
    $g = new StubGenerator($n);
    $g->generate($argv[2] ?? STDOUT);
})();
