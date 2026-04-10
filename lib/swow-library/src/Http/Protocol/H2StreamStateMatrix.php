<?php

declare(strict_types=1);

namespace Swow\Http\Protocol;

final class H2StreamStateMatrix
{
    public static function assertCanSend(int $streamId, ?string $state): void
    {
        if ($state === H2Connection::STREAM_STATE_HALF_CLOSED_LOCAL || $state === H2Connection::STREAM_STATE_CLOSED) {
            throw H2Exception::forStreamError(
                $streamId,
                'Cannot send on locally closed stream',
                H2Exception::ERROR_STREAM_CLOSED
            );
        }
    }

    public static function assertCanReceiveData(int $streamId, ?string $state): void
    {
        if ($state === H2Connection::STREAM_STATE_HALF_CLOSED_REMOTE || $state === H2Connection::STREAM_STATE_CLOSED) {
            throw H2Exception::forStreamError(
                $streamId,
                'Cannot receive DATA on remotely closed stream',
                H2Exception::ERROR_STREAM_CLOSED
            );
        }
    }

    public static function validateInboundFrame(
        H2Frame $frame,
        ?string $state,
        bool $isClosed,
        bool $isIdleDataFrame,
        bool $isIdleContinuationFrame,
    ): H2Frame {
        if (
            ($frame->type === H2Frame::TYPE_DATA ||
             $frame->type === H2Frame::TYPE_HEADERS ||
             $frame->type === H2Frame::TYPE_CONTINUATION) &&
            ($frame->streamId < 1 || ($frame->streamId % 2) === 0)
        ) {
            throw H2Exception::forConnectionError('Request frame must use an odd positive stream ID');
        }

        if ($isIdleDataFrame || $isIdleContinuationFrame) {
            throw H2Exception::forConnectionError('Frame received for idle stream');
        }

        if (
            $frame->streamId > 0 &&
            ($isClosed || $state === H2Connection::STREAM_STATE_HALF_CLOSED_REMOTE) &&
            ($frame->type === H2Frame::TYPE_DATA ||
             $frame->type === H2Frame::TYPE_HEADERS ||
             $frame->type === H2Frame::TYPE_CONTINUATION)
        ) {
            throw H2Exception::forStreamError(
                $frame->streamId,
                'Frame received for closed stream',
                H2Exception::ERROR_STREAM_CLOSED
            );
        }

        return $frame;
    }

    public static function stateAfterRemoteOpened(bool $ended): string
    {
        return $ended
            ? H2Connection::STREAM_STATE_HALF_CLOSED_REMOTE
            : H2Connection::STREAM_STATE_OPEN;
    }

    public static function stateAfterRemoteEnded(?string $state): string
    {
        if ($state === H2Connection::STREAM_STATE_HALF_CLOSED_LOCAL) {
            return H2Connection::STREAM_STATE_CLOSED;
        }

        return H2Connection::STREAM_STATE_HALF_CLOSED_REMOTE;
    }

    public static function stateAfterLocalEnded(?string $state): string
    {
        if ($state === H2Connection::STREAM_STATE_HALF_CLOSED_REMOTE) {
            return H2Connection::STREAM_STATE_CLOSED;
        }

        return H2Connection::STREAM_STATE_HALF_CLOSED_LOCAL;
    }
}
