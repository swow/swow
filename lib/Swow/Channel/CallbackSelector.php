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

namespace Swow\Channel;

use Swow\Channel;

class CallbackSelector extends Selector
{
    protected $pushCallbacks = [];

    protected $popCallbacks = [];

    public function casePush(Channel $channel, $data, callable $callback): self
    {
        $this->pushCallbacks[spl_object_id($channel)] = $callback;

        return $this->push($channel, $data);
    }

    public function casePop(Channel $channel, callable $callback): self
    {
        $this->popCallbacks[spl_object_id($channel)] = $callback;

        return $this->pop($channel);
    }

    public function select(int $timeout = -1): void
    {
        $channel = parent::do($timeout);
        $id = spl_object_id($channel);
        if ($this->getLastOpcode() === Channel::OPCODE_PUSH) {
            $this->pushCallbacks[$id]();
        } else {
            $this->popCallbacks[$id]($this->fetch());
        }
    }
}
