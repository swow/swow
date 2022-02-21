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

namespace Swow\Http;

use Swow\WebSocket;

class WebSocketFrame extends WebSocket\Frame
{
    public function getPayloadData(): Buffer
    {
        if (!$this->hasPayloadData()) {
            $payloadData = Buffer::for();
            $this->setPayloadData($payloadData);

            return $payloadData;
        }

        return Buffer::for(parent::getPayloadData());
    }

    public function setPayloadData(mixed $buffer): static
    {
        return parent::setPayloadData(Buffer::for($buffer));
    }
}
