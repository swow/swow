#!/usr/bin/env php
<?php
/**
 * This file is part of Swow
 *
 * @link     https://github.com/swow/swow
 * @contact  twosee <twose@qq.com>
 *
 * For the full copyright and license information,
 * please view the LICENSE file that was distributed with this source code
 */

require __DIR__ . '/../tools/dm.php';

use Swow\Tools\DependencyManager;
use function Swow\Tools\error;
use function Swow\Tools\log;
use function Swow\Tools\ok;

try {
    log('Sync libcat...');
    $libcatVersion = DependencyManager::sync(
        'libcat',
        'git@github.com:libcat/libcat.git',
        'libcat',
        __DIR__ . '/libcat'
    );
    ok("Sync libcat to {$libcatVersion}");
} catch (Exception $exception) {
    error($exception->getMessage());
}
