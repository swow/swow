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

namespace Swow\Psr7;

use function array_merge;

trait ServerParamsTrait
{
    /** @var array{'remote_addr': string, 'remote_port': int} */
    protected array $serverParams = [];

    public function getServerParams(): array
    {
        return $this->serverParams;
    }

    public function setServerParams(array $serverParams): static
    {
        $this->serverParams = $serverParams;

        return $this;
    }

    public function addServerParam(string $key, string $value): static
    {
        $this->serverParams[$key] = $value;

        return $this;
    }

    public function addServerParams(array $serverParams): static
    {
        $this->serverParams = array_merge($this->serverParams, $serverParams);

        return $this;
    }

    public function unsetServerParam(string $key): static
    {
        unset($this->serverParams[$key]);

        return $this;
    }
}
