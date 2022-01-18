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
use TypeError;

class WebSocketFrame extends WebSocket\Frame
{
    public function getPayloadData(): Buffer
    {
        if (!$this->hasPayloadData()) {
            $payloadData = new Buffer();
            $this->setPayloadData($payloadData);

            return $payloadData;
        }

        /* @noinspection PhpIncompatibleReturnTypeInspection */
        return parent::getPayloadData();
    }

    /**
     * @param Buffer|null $buffer
     */
    public function setPayloadData(?\Swow\Buffer $buffer): static
    {
        if (!($buffer instanceof Buffer)) {
            throw new TypeError('$buffer should be an instance of ' . Buffer::class);
        }

        return parent::setPayloadData($buffer);
    }
}
