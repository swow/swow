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

use Swow\Http\Client;
use Swow\Http\Request;

require __DIR__ . '/../autoload.php';

$client = new Client();
$domain = 'www.baidu.com';
$request = new Request('GET', '/', [
    'Host' => $domain,
]);
$response = $client
    ->connect($domain, 80)
    ->sendRequest($request);

echo 'Request', PHP_EOL, str_repeat('-', 40), PHP_EOL, $request;
echo 'Response', PHP_EOL, str_repeat('-', 40), PHP_EOL, $response;
