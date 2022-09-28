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

require __DIR__ . '/../autoload.php';

use Swow\Psr7\Client\Client;
use Swow\Psr7\Message\Request;
use Swow\Psr7\Message\WebSocketFrame;

$client = new Client();

/* do handshake */
$request = new Request('GET', '/chat');
$response = $client
    ->connect('127.0.0.1', 9764)
    ->upgradeToWebSocket($request);

/* chat */
for ($n = 1; $n <= 3; $n++) {
    $message = new WebSocketFrame();
    $message->getPayloadData()->write("Hello Swow {$n}");
    echo 'Send Frame:' . PHP_EOL;
    var_dump($message);
    $reply = $client
        ->sendWebSocketFrame($message)
        ->recvWebSocketFrame();
    echo 'Recv Frame:' . PHP_EOL;
    var_dump($reply);
    echo PHP_EOL;
    sleep(1);
}
