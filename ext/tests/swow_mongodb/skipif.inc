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

require __DIR__ . '/../include/skipif.php';
skip_if_env_not_true('TEST_SWOW_MONGODB');
skip_if_extension_not_exist('mongodb');
skip_if(!Swow\Extension::isBuiltWith('mongodb'), 'MongoDB runtime adapter is not built in');
skip_if_ini_bool_equal_to('swow.enable', false);

try {
    $probe = new MongoDB\Driver\Manager(
        getenv('MONGODB_URI') ?: 'mongodb://127.0.0.1/',
        ['serverSelectionTimeoutMS' => 1000, 'connectTimeoutMS' => 1000]
    );
    $probe->executeCommand('admin', new MongoDB\Driver\Command(['ping' => 1]))->toArray();
} catch (MongoDB\Driver\Exception\Exception $exception) {
    skip('MongoDB test server is unavailable: ' . $exception->getMessage());
}
