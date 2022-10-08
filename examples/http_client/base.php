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

use Swow\Psr7\Client\Client;
use Swow\Psr7\Psr7;

require __DIR__ . '/../autoload.php';

$client = new Client();
$domain = 'www.baidu.com';
$request = Psr7::createRequest(method: 'GET', uri: '/');
$response = $client
    ->connect($domain, 80)
    ->sendRequest($request);

echo 'Request', PHP_EOL, str_repeat('-', 40), PHP_EOL, Psr7::stringifyRequest($request);
echo 'Response', PHP_EOL, str_repeat('-', 40), PHP_EOL, Psr7::stringifyResponse($response);
