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
use Stringable;

interface WebSocketFrameInterface extends Stringable
{
    public function getOpcode(): int;

    public function setOpcode(int $opcode): static;

    public function withOpcode(int $opcode): static;

    public function getFin(): bool;

    public function setFin(bool $fin): static;

    public function withFin(bool $fin): static;

    public function getRSV1(): bool;

    public function setRSV1(bool $rsv1): static;

    public function withRSV1(bool $rsv1): static;

    public function getRSV2(): bool;

    public function setRSV2(bool $rsv2): static;

    public function withRSV2(bool $rsv2): static;

    public function getRSV3(): bool;

    public function setRSV3(bool $rsv3): static;

    public function withRSV3(bool $rsv3): static;

    public function getPayloadLength(): int;

    public function setPayloadLength(int $payloadLength): static;

    public function withPayloadLength(int $payloadLength): static;

    public function getMask(): bool;

    public function getMaskingKey(): string;

    public function setMaskingKey(string $maskingKey): static;

    public function withMaskingKey(string $maskingKey): static;

    public function getPayloadData(): StreamInterface;

    public function setPayloadData(mixed $payloadData): static;

    public function withPayloadData(mixed $payloadData): static;

    public function toString(bool $withoutPayloadData = false): string;
}
