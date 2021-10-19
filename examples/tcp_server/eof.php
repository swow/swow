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

require __DIR__ . '/../../tools/autoload.php';

use Swow\Coroutine;
use Swow\Stream\EofStream;

$server = new EofStream();
$server->bind('127.0.0.1', 9764)->listen();
while (true) {
    Coroutine::run(function (EofStream $stream) {
        echo "Stream<fd={$stream->getFd()}> accepted" . PHP_EOL;
        try {
            while (true) {
                $packet = $stream->recvPacketString();
                echo "Stream<fd={$stream->getFd()}>: \"{$packet}\"" . PHP_EOL;
                $stream->write([$packet, $stream->getEof()]);
            }
        } catch (\Swow\Socket\Exception $exception) {
            echo "Stream<fd={$stream->getFd()}> goaway, reason: {$exception->getMessage()}" . PHP_EOL;
        }
    }, $server->accept());
}
