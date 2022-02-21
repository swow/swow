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

namespace Swow\Http\Server;

class Response extends \Swow\Http\Response
{
    protected array $headers = ['Server' => ['swow']];

    public function error(int $statusCode, string $reasonPhrase = ''): static
    {
        $this
            ->setStatus($statusCode, $reasonPhrase)
            ->getBody()->clear()
            ->write('<html lang="en"><body><h2>HTTP ')->write((string) $statusCode)->write(' ')->write($reasonPhrase)->write('</h2><hr><i>Powered by Swow</i></body></html>');

        return $this;
    }
}
