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

namespace Swow\Channel;

use Closure;
use Swow\Channel;
use function spl_object_id;

class CallbackSelector extends Selector
{
    /** @var array<Closure> */
    protected array $pushCallbacks = [];

    /** @var array<Closure> */
    protected array $popCallbacks = [];

    public function casePush(Channel $channel, mixed $data, callable $callback): static
    {
        $this->pushCallbacks[spl_object_id($channel)] = Closure::fromCallable($callback);

        return $this->push($channel, $data);
    }

    public function casePop(Channel $channel, callable $callback): static
    {
        $this->popCallbacks[spl_object_id($channel)] = Closure::fromCallable($callback);

        return $this->pop($channel);
    }

    public function select(int $timeout = -1): void
    {
        $channel = $this->commit($timeout);
        $id = spl_object_id($channel);
        if ($this->getLastOpcode() === Channel::OPCODE_PUSH) {
            $this->pushCallbacks[$id]();
        } else {
            $this->popCallbacks[$id]($this->fetch());
        }
    }
}
