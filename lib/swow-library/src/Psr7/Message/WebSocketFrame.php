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

namespace Swow\Psr7\Message;

use Psr\Http\Message\StreamInterface;
use Swow\Object\DupTrait;
use Swow\Psr7\Psr7;
use Swow\WebSocket;

class WebSocketFrame extends WebSocket\Header
{
    use DupTrait;

    protected ?StreamInterface $payloadData = null;

    public function __construct(
        bool $fin = true,
        bool $rsv1 = false,
        bool $rsv2 = false,
        bool $rsv3 = false,
        int $opcode = \Swow\WebSocket\Opcode::TEXT,
        int $payloadLength = 0,
        string $maskKey = '',
        mixed $payloadData = '',
    ) {
        parent::__construct($fin, $rsv1, $rsv2, $rsv3, $opcode, $payloadLength, $maskKey);
        if ($payloadData !== null && $payloadData !== '') {
            $this->setPayloadData($payloadData);
        }
    }

    protected function updateHeader(): void
    {
        $this->setPayloadLength($this->getPayloadData()->getSize());
    }

    public function getHeaderSize(): int
    {
        $this->updateHeader();
        return parent::getHeaderSize();
    }

    public function getPayloadLength(): int
    {
        return $this->getPayloadData()->getSize();
    }

    public function getPayloadData(): StreamInterface
    {
        return $this->payloadData ??= Psr7::createStream();
    }

    public function setPayloadData(mixed $payloadData): static
    {
        $this->payloadData = Psr7::createStreamFromAny($payloadData);

        return $this;
    }

    public function toString(bool $withoutPayloadData = false): string
    {
        $this->updateHeader();
        $string = parent::toString();
        if (!$withoutPayloadData && $this->payloadData !== null) {
            $string .= ((string) $this->payloadData);
        }
        return $string;
    }

    public function __toString(): string
    {
        return parent::toString();
    }
}
