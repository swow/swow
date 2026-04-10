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

namespace Swow\Http\Protocol;

use RuntimeException;
use Swow\Buffer;
use Swow\Socket;
use Swow\SocketException;
use Throwable;

use function pack;
use function strlen;
use function substr;
use function unpack;

class H2Connection
{
    public const CLIENT_PREFACE = "PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n";
    public const DEFAULT_INITIAL_WINDOW_SIZE = 65_535;
    public const DEFAULT_WINDOW_UPDATE_THRESHOLD = 32_768;
    public const SETTINGS_INITIAL_WINDOW_SIZE = 0x4;
    public const SETTINGS_MAX_FRAME_SIZE = 0x5;
    public const MIN_MAX_FRAME_SIZE = 16_384;
    public const MAX_MAX_FRAME_SIZE = 16_777_215;
    public const STREAM_STATE_OPEN = 'open';
    public const STREAM_STATE_HALF_CLOSED_REMOTE = 'half_closed_remote';
    public const STREAM_STATE_HALF_CLOSED_LOCAL = 'half_closed_local';
    public const STREAM_STATE_CLOSED = 'closed';

    protected Buffer $buffer;

    protected int $bufferedLength = 0;

    protected int $localMaxFrameSize = H2Frame::DEFAULT_MAX_FRAME_SIZE;

    protected int $streamInitialWindowSize = self::DEFAULT_INITIAL_WINDOW_SIZE;

    protected int $streamSendInitialWindowSize = self::DEFAULT_INITIAL_WINDOW_SIZE;

    protected int $connectionReceiveWindow = self::DEFAULT_INITIAL_WINDOW_SIZE;

    protected int $connectionSendWindow = self::DEFAULT_INITIAL_WINDOW_SIZE;

    /** @var array<int, int> */
    protected array $streamReceiveWindows = [];

    /** @var array<int, int> */
    protected array $streamSendWindows = [];

    /** @var array<int, int> */
    protected array $connectionBytesConsumed = [];

    /** @var array<int, int> */
    protected array $streamBytesConsumed = [];

    protected int $lastProcessedStreamId = 0;

    protected int $lastAllowedStreamId = H2Frame::MAX_STREAM_ID;

    protected bool $goAwaySent = false;

    protected bool $goAwayReceived = false;

    /** @var array<int, true> */
    protected array $closedStreams = [];

    /** @var array<int, self::STREAM_STATE_*> */
    protected array $streamStates = [];

    protected H2PendingFrameQueue $pendingFrames;

    protected H2FrameScheduler $scheduler;

    protected ?int $activeHeaderBlockStreamId = null;

    protected bool $initialized = false;

    public function __construct(
        protected Socket $socket,
        protected int $bufferSize = 65_536,
    ) {
        $this->buffer = new Buffer($bufferSize);
        $this->pendingFrames = new H2PendingFrameQueue();
        $this->scheduler = new H2FrameScheduler($this->pendingFrames, $this->isSendWindowControlFrame(...));
    }

    public function getSocket(): Socket
    {
        return $this->socket;
    }

    public function isInitialized(): bool
    {
        return $this->initialized;
    }

    public function getLocalMaxFrameSize(): int
    {
        return $this->localMaxFrameSize;
    }

    public function getLastProcessedStreamId(): int
    {
        return $this->lastProcessedStreamId;
    }

    public function getLastAllowedStreamId(): int
    {
        return $this->lastAllowedStreamId;
    }

    public function hasSentGoAway(): bool
    {
        return $this->goAwaySent;
    }

    public function hasReceivedGoAway(): bool
    {
        return $this->goAwayReceived;
    }

    public function isStreamClosed(int $streamId): bool
    {
        return isset($this->closedStreams[$streamId]);
    }

    public function getStreamState(int $streamId): string|null
    {
        return $this->streamStates[$streamId] ?? null;
    }

    public function getConnectionSendWindow(): int
    {
        return $this->connectionSendWindow;
    }

    public function getStreamSendWindow(int $streamId): int
    {
        return $this->streamSendWindows[$streamId] ?? $this->streamSendInitialWindowSize;
    }

    public function getAvailableSendWindow(int $streamId): int
    {
        return min($this->connectionSendWindow, $this->getStreamSendWindow($streamId));
    }

    public function consumeSendWindow(int $streamId, int $bytes): void
    {
        if ($bytes === 0) {
            return;
        }

        if ($this->connectionSendWindow < $bytes) {
            throw H2Exception::forConnectionError(
                'Connection send window exhausted',
                H2Exception::ERROR_FLOW_CONTROL_ERROR
            );
        }

        $streamWindow = $this->getStreamSendWindow($streamId);
        if ($streamWindow < $bytes) {
            throw H2Exception::forStreamError(
                $streamId,
                'Stream send window exhausted',
                H2Exception::ERROR_FLOW_CONTROL_ERROR
            );
        }

        $this->connectionSendWindow -= $bytes;
        $this->streamSendWindows[$streamId] = $streamWindow - $bytes;
    }

    public function readClientPreface(?int $timeout = null): void
    {
        $needed = strlen(self::CLIENT_PREFACE);
        $preface = $this->readBytes($needed, $timeout);

        if ($preface !== self::CLIENT_PREFACE) {
            throw H2Exception::forConnectionError('Invalid client connection preface');
        }
    }

    public function initialize(?int $timeout = null, array $settings = []): void
    {
        $this->sendSettings($settings, $timeout);
        $this->initialized = true;
    }

    public function initializeForUpgrade(?int $timeout = null, array $settings = [], string $peerSettingsPayload = ''): void
    {
        if ($peerSettingsPayload !== '') {
            $this->applySettingsPayload($peerSettingsPayload);
        }

        $this->sendSettings($settings, $timeout);
        if ($peerSettingsPayload !== '') {
            $this->sendSettings(timeout: $timeout, ack: true);
        }
        $this->initialized = true;
    }

    public function readFrame(?int $timeout = null): H2Frame
    {
        $header = $this->readBytes(H2Frame::HEADER_LENGTH, $timeout);
        $metadata = H2Frame::decodeHeader($header);

        if ($metadata['length'] > $this->localMaxFrameSize) {
            throw H2Exception::forConnectionError(
                'Received frame larger than local max frame size',
                H2Exception::ERROR_FRAME_SIZE_ERROR
            );
        }

        $payload = $this->readBytes($metadata['length'], $timeout);

        return $this->validateFrame(new H2Frame(
            $metadata['type'],
            $metadata['flags'],
            $metadata['streamId'],
            $payload
        ));
    }

    public function waitForSendWindow(int $streamId, int $minimum = 1, ?int $timeout = null): void
    {
        while ($this->getAvailableSendWindow($streamId) < $minimum) {
            try {
                $frame = $this->takePendingControlFrame() ?? $this->readFrame($timeout);
            } catch (Throwable $exception) {
                if ($this->connectionSendWindow === 0) {
                    throw H2Exception::forConnectionError(
                        'Connection send window exhausted',
                        H2Exception::ERROR_FLOW_CONTROL_ERROR,
                        $exception
                    );
                }

                if ($this->getStreamSendWindow($streamId) === 0) {
                    throw H2Exception::forStreamError(
                        $streamId,
                        'Stream send window exhausted',
                        H2Exception::ERROR_FLOW_CONTROL_ERROR,
                        $exception
                    );
                }

                throw $exception;
            }

            if ($this->isSendWindowControlFrame($frame)) {
                $this->pumpControlFrame($frame, $timeout);
                $this->assertCanSendOnStream($streamId);
                continue;
            }

            $this->deferFrame($frame);
        }
    }

    public function validateFrame(H2Frame $frame): H2Frame
    {
        return match ($frame->type) {
            H2Frame::TYPE_SETTINGS => $this->validateSettingsFrame($frame),
            H2Frame::TYPE_PING => $this->validatePingFrame($frame),
            H2Frame::TYPE_GOAWAY => $this->validateGoAwayFrame($frame),
            H2Frame::TYPE_WINDOW_UPDATE => $this->validateWindowUpdateFrame($frame),
            H2Frame::TYPE_RST_STREAM => $this->validateRstStreamFrame($frame),
            H2Frame::TYPE_PRIORITY => $this->validatePriorityFrame($frame),
            H2Frame::TYPE_PUSH_PROMISE => $this->validatePushPromiseFrame($frame),
            default => $this->validateOpenStreamFrame($frame),
        };
    }

    public function readHeaderBlock(?int $timeout = null): H2HeaderBlock
    {
        $headerBlock = $this->readHeaderBlockOrNull($timeout);
        if (!$headerBlock instanceof H2HeaderBlock) {
            throw H2Exception::forConnectionError('No more request streams are allowed on this connection');
        }

        return $headerBlock;
    }

    public function readHeaderBlockOrNull(?int $timeout = null): ?H2HeaderBlock
    {
        $frame = $this->readRequestStartFrame($timeout);
        if (!$frame instanceof H2Frame) {
            return null;
        }

        $this->assertCanAcceptRequestStream($frame->streamId);

        if ($frame->payload === '') {
            throw H2Exception::forConnectionError('HEADERS frame payload must not be empty');
        }

        $headerBlock = new H2HeaderBlock($frame->streamId);
        $headerBlock->append($frame);
        $this->activeHeaderBlockStreamId = $frame->streamId;
        try {
            while (!$headerBlock->isCompleted()) {
                $continuation = $this->nextRelevantFrameForStream($frame->streamId, $timeout);
                $headerBlock->append($continuation);
            }
        } finally {
            $this->activeHeaderBlockStreamId = null;
        }

        $this->noteRemoteStreamOpened($frame->streamId, ($headerBlock->getFlags() & H2Frame::FLAG_END_STREAM) !== 0);
        $this->markRequestStreamProcessed($frame->streamId);

        return $headerBlock;
    }

    public function readRequestStartFrame(?int $timeout = null): ?H2Frame
    {
        return $this->scheduler->nextRequestStartFrame(
            $this->readFrame(...),
            $this->pumpControlFrame(...),
            $this->canAcceptNewRequests(...),
            $this->isGracefulRequestStartTermination(...),
            $timeout
        );
    }

    public function canAcceptNewRequests(): bool
    {
        return !$this->goAwayReceived || $this->lastProcessedStreamId < $this->lastAllowedStreamId;
    }

    public function readRequestBody(int $streamId, ?int $timeout = null): string
    {
        return $this->readRequestBodyWithTrailers($streamId, $timeout)['body'];
    }

    /**
     * @return array{body: string, trailers: H2HeaderBlock|null}
     */
    public function readRequestBodyWithTrailers(int $streamId, ?int $timeout = null): array
    {
        if ($streamId < 1) {
            throw H2Exception::forConnectionError('Request body stream ID must be positive');
        }

        $this->assertCanReceiveDataOnStream($streamId);
        $this->streamReceiveWindows[$streamId] ??= $this->streamInitialWindowSize;
        $this->connectionBytesConsumed[$streamId] ??= 0;
        $this->streamBytesConsumed[$streamId] ??= 0;

        $body = '';
        while (true) {
            $frame = $this->nextRelevantFrameForStream($streamId, $timeout);
            if ($this->isSendWindowControlFrame($frame)) {
                $this->pumpControlFrame($frame, $timeout);
                $this->assertCanReceiveDataOnStream($streamId);
                continue;
            }

            if ($frame->type === H2Frame::TYPE_DATA) {
                $this->consumeReceiveWindow($streamId, strlen($frame->payload));
                $body .= $frame->payload;
                if (($frame->flags & H2Frame::FLAG_END_STREAM) !== 0) {
                    $this->noteRemoteStreamEnded($streamId);
                    return ['body' => $body, 'trailers' => null];
                }

                continue;
            }

            if ($frame->type === H2Frame::TYPE_HEADERS) {
                $trailerBlock = $this->readTrailerHeaderBlock($frame, $timeout);
                $this->noteRemoteStreamEnded($streamId);
                return ['body' => $body, 'trailers' => $trailerBlock];
            }

            throw H2Exception::forConnectionError('Expected DATA or trailing HEADERS frame while reading request body');
        }
    }

    public function sendFrame(H2Frame $frame, ?int $timeout = null): void
    {
        try {
            $this->socket->send($frame->encode(), timeout: $timeout);
        } catch (SocketException $exception) {
            throw H2Exception::forConnectionError(
                'Failed to send H2 frame: ' . $exception->getMessage(),
                H2Exception::ERROR_INTERNAL_ERROR,
                $exception
            );
        }
    }

    public function sendSettings(array $settings = [], ?int $timeout = null, bool $ack = false): void
    {
        $payload = '';
        foreach ($settings as $identifier => $value) {
            $payload .= pack('nN', $identifier, $value);
        }

        $this->sendFrame(
            new H2Frame(
                H2Frame::TYPE_SETTINGS,
                $ack ? H2Frame::FLAG_ACK : 0,
                0,
                $payload
            ),
            $timeout
        );
    }

    public function sendPingAck(string $opaqueData, ?int $timeout = null): void
    {
        $this->sendFrame(
            new H2Frame(
                H2Frame::TYPE_PING,
                H2Frame::FLAG_ACK,
                0,
                $this->normalizePingOpaqueData($opaqueData)
            ),
            $timeout
        );
    }

    public function goAway(
        int $errorCode = H2Exception::ERROR_NO_ERROR,
        int $lastStreamId = 0,
        string $debugData = '',
        ?int $timeout = null
    ): void {
        if ($lastStreamId < 0 || $lastStreamId > H2Frame::MAX_STREAM_ID) {
            throw H2Exception::forConnectionError('GOAWAY last stream ID out of range');
        }

        $payload = pack('NN', $lastStreamId & H2Frame::MAX_STREAM_ID, $errorCode) . $debugData;
        $this->goAwaySent = true;
        $this->lastAllowedStreamId = min($this->lastAllowedStreamId, $lastStreamId);
        $this->sendFrame(new H2Frame(H2Frame::TYPE_GOAWAY, 0, 0, $payload), $timeout);
    }

    public function sendRstStream(
        int $streamId,
        int $errorCode = H2Exception::ERROR_PROTOCOL_ERROR,
        ?int $timeout = null
    ): void {
        $this->markStreamClosed($streamId);
        $this->sendFrame(H2Frame::createRstStream($streamId, $errorCode), $timeout);
    }

    public function noteResponseEnd(int $streamId): void
    {
        $this->noteLocalStreamEnded($streamId);
    }

    public function beginUpgradedRequestStream(int $streamId = 1, bool $remoteEnded = true): void
    {
        $this->assertCanAcceptRequestStream($streamId);
        $this->noteRemoteStreamOpened($streamId, $remoteEnded);
        $this->markRequestStreamProcessed($streamId);
    }

    public function assertCanAcceptRequestStream(int $streamId): void
    {
        if ($streamId < 1 || ($streamId % 2) === 0) {
            throw H2Exception::forConnectionError('Request HEADERS frame must use an odd positive stream ID');
        }

        if ($this->isExistingRequestStream($streamId)) {
            return;
        }

        $this->assertStreamAllowedAfterGoAway($streamId);
    }

    public function markRequestStreamProcessed(int $streamId): void
    {
        $this->lastProcessedStreamId = max($this->lastProcessedStreamId, $streamId);
    }

    public function isExistingRequestStream(int $streamId): bool
    {
        return $streamId > 0 && $streamId <= $this->lastProcessedStreamId && !$this->isStreamClosed($streamId);
    }

    public function assertCanUseResponseStream(int $streamId): void
    {
        if ($streamId < 1) {
            throw H2Exception::forConnectionError('Response stream ID must be positive');
        }

        $this->assertCanSendOnStream($streamId);

        if ($this->isExistingRequestStream($streamId)) {
            return;
        }

        $this->assertStreamAllowedAfterGoAway($streamId);
    }

    public function assertCanSendOnStream(int $streamId): void
    {
        H2StreamStateMatrix::assertCanSend($streamId, $this->streamStates[$streamId] ?? null);
    }

    public function assertCanReceiveDataOnStream(int $streamId): void
    {
        H2StreamStateMatrix::assertCanReceiveData($streamId, $this->streamStates[$streamId] ?? null);
    }

    private function assertStreamAllowedAfterGoAway(int $streamId): void
    {
        if (($this->goAwaySent || $this->goAwayReceived) && $streamId > $this->lastAllowedStreamId) {
            throw H2Exception::forStreamError(
                $streamId,
                'New stream is not allowed after GOAWAY',
                H2Exception::ERROR_REFUSED_STREAM
            );
        }
    }

    public function markStreamClosed(int $streamId): void
    {
        if ($streamId > 0) {
            $this->closedStreams[$streamId] = true;
            $this->streamStates[$streamId] = self::STREAM_STATE_CLOSED;
        }
    }

    protected function readBytes(int $length, ?int $timeout = null): string
    {
        if ($length === 0) {
            return '';
        }

        while ($this->bufferedLength < $length) {
            $read = $this->recvToBuffer($timeout);
            if ($read <= 0) {
                throw H2Exception::forConnectionError('Connection closed while reading HTTP/2 bytes');
            }
        }

        $chunk = $this->buffer->read(0, $length);
        $remaining = $this->bufferedLength - $length;

        if ($remaining > 0) {
            $tail = $this->buffer->read($length, $remaining);
            $this->buffer->clear();
            $this->buffer->append($tail);
            $this->bufferedLength = $remaining;
        } else {
            $this->buffer->clear();
            $this->bufferedLength = 0;
        }

        return $chunk;
    }

    protected function recvToBuffer(?int $timeout = null): int
    {
        $free = $this->buffer->getSize() - $this->bufferedLength;
        if ($free <= 0) {
            throw H2Exception::forConnectionError('H2 receive buffer exhausted');
        }

        try {
            $read = $this->socket->recv($this->buffer, $this->bufferedLength, $free, $timeout);
        } catch (SocketException $exception) {
            throw H2Exception::forConnectionError(
                'Failed to receive H2 bytes: ' . $exception->getMessage(),
                H2Exception::ERROR_INTERNAL_ERROR,
                $exception
            );
        }

        $this->bufferedLength += $read;

        return $read;
    }

    protected function normalizePingOpaqueData(string $opaqueData): string
    {
        $length = strlen($opaqueData);
        if ($length === 8) {
            return $opaqueData;
        }

        if ($length > 8) {
            return substr($opaqueData, 0, 8);
        }

        return $opaqueData . str_repeat("\x00", 8 - $length);
    }

    protected function consumeReceiveWindow(int $streamId, int $bytes): void
    {
        if ($bytes === 0) {
            return;
        }

        if ($this->connectionReceiveWindow < $bytes) {
            throw H2Exception::forConnectionError('Connection receive window exhausted', H2Exception::ERROR_FLOW_CONTROL_ERROR);
        }

        $streamWindow = $this->streamReceiveWindows[$streamId] ?? $this->streamInitialWindowSize;
        if ($streamWindow < $bytes) {
            throw H2Exception::forStreamError(
                $streamId,
                'Stream receive window exhausted',
                H2Exception::ERROR_FLOW_CONTROL_ERROR
            );
        }

        $this->connectionReceiveWindow -= $bytes;
        $this->streamReceiveWindows[$streamId] = $streamWindow - $bytes;
        $this->connectionBytesConsumed[$streamId] = ($this->connectionBytesConsumed[$streamId] ?? 0) + $bytes;
        $this->streamBytesConsumed[$streamId] = ($this->streamBytesConsumed[$streamId] ?? 0) + $bytes;

        if ($this->connectionBytesConsumed[$streamId] >= self::DEFAULT_WINDOW_UPDATE_THRESHOLD) {
            $increment = $this->connectionBytesConsumed[$streamId];
            $this->connectionBytesConsumed[$streamId] = 0;
            $this->connectionReceiveWindow += $increment;
            $this->sendFrame(H2Frame::createWindowUpdate(0, $increment));
        }

        if ($this->streamBytesConsumed[$streamId] >= self::DEFAULT_WINDOW_UPDATE_THRESHOLD) {
            $increment = $this->streamBytesConsumed[$streamId];
            $this->streamBytesConsumed[$streamId] = 0;
            $this->streamReceiveWindows[$streamId] += $increment;
            $this->sendFrame(H2Frame::createWindowUpdate($streamId, $increment));
        }
    }

    private function validateSettingsFrame(H2Frame $frame): H2Frame
    {
        if ($frame->streamId !== 0) {
            throw H2Exception::forConnectionError('SETTINGS frame must use stream 0');
        }

        if (($frame->flags & H2Frame::FLAG_ACK) !== 0) {
            if ($frame->payload !== '') {
                throw H2Exception::forConnectionError(
                    'SETTINGS ACK frame must not contain a payload',
                    H2Exception::ERROR_FRAME_SIZE_ERROR
                );
            }

            return $frame;
        }

        if ((strlen($frame->payload) % 6) !== 0) {
            throw H2Exception::forConnectionError(
                'SETTINGS payload length must be a multiple of 6',
                H2Exception::ERROR_FRAME_SIZE_ERROR
            );
        }

        $this->applySettingsPayload($frame->payload);

        return $frame;
    }

    private function applySettingsPayload(string $payload): void
    {
        $length = strlen($payload);
        for ($offset = 0; $offset < $length; $offset += 6) {
            $setting = unpack('nidentifier/Nvalue', substr($payload, $offset, 6));
            $identifier = $setting['identifier'] ?? 0;
            $value = $setting['value'] ?? 0;

            match ($identifier) {
                self::SETTINGS_INITIAL_WINDOW_SIZE => $this->applyInitialWindowSize($value),
                self::SETTINGS_MAX_FRAME_SIZE => $this->applyMaxFrameSize($value),
                default => null,
            };
        }
    }

    private function applyInitialWindowSize(int $value): void
    {
        if ($value > H2Frame::MAX_WINDOW_SIZE) {
            throw H2Exception::forConnectionError(
                'SETTINGS_INITIAL_WINDOW_SIZE exceeds maximum allowed value',
                H2Exception::ERROR_FLOW_CONTROL_ERROR
            );
        }

        $delta = $value - $this->streamInitialWindowSize;
        $sendDelta = $value - $this->streamSendInitialWindowSize;
        $this->streamInitialWindowSize = $value;
        $this->streamSendInitialWindowSize = $value;

        foreach ($this->streamReceiveWindows as $streamId => $window) {
            $nextWindow = $window + $delta;
            if ($nextWindow > H2Frame::MAX_WINDOW_SIZE) {
                throw H2Exception::forStreamError(
                    $streamId,
                    'Adjusted stream receive window exceeds maximum allowed value',
                    H2Exception::ERROR_FLOW_CONTROL_ERROR
                );
            }

            $this->streamReceiveWindows[$streamId] = $nextWindow;
        }

        foreach ($this->streamSendWindows as $streamId => $window) {
            $nextWindow = $window + $sendDelta;
            if ($nextWindow > H2Frame::MAX_WINDOW_SIZE) {
                throw H2Exception::forStreamError(
                    $streamId,
                    'Adjusted stream send window exceeds maximum allowed value',
                    H2Exception::ERROR_FLOW_CONTROL_ERROR
                );
            }

            $this->streamSendWindows[$streamId] = $nextWindow;
        }
    }

    private function applyMaxFrameSize(int $value): void
    {
        if ($value < self::MIN_MAX_FRAME_SIZE || $value > self::MAX_MAX_FRAME_SIZE) {
            throw H2Exception::forConnectionError(
                'SETTINGS_MAX_FRAME_SIZE out of allowed range',
                H2Exception::ERROR_PROTOCOL_ERROR
            );
        }

        $this->localMaxFrameSize = $value;
    }

    private function validatePingFrame(H2Frame $frame): H2Frame
    {
        if ($frame->streamId !== 0) {
            throw H2Exception::forConnectionError('PING frame must use stream 0');
        }

        if (strlen($frame->payload) !== 8) {
            throw H2Exception::forConnectionError(
                'PING payload length must be exactly 8',
                H2Exception::ERROR_FRAME_SIZE_ERROR
            );
        }

        return $frame;
    }

    private function validatePriorityFrame(H2Frame $frame): H2Frame
    {
        if ($frame->streamId < 1) {
            throw H2Exception::forConnectionError('PRIORITY frame must use a positive stream ID');
        }

        if (strlen($frame->payload) !== 5) {
            throw H2Exception::forConnectionError(
                'PRIORITY payload length must be exactly 5',
                H2Exception::ERROR_FRAME_SIZE_ERROR
            );
        }

        return $frame;
    }

    private function validatePushPromiseFrame(H2Frame $frame): H2Frame
    {
        throw H2Exception::forConnectionError(
            'Client must not send PUSH_PROMISE frames',
            H2Exception::ERROR_PROTOCOL_ERROR
        );
    }

    private function validateGoAwayFrame(H2Frame $frame): H2Frame
    {
        if ($frame->streamId !== 0) {
            throw H2Exception::forConnectionError('GOAWAY frame must use stream 0');
        }

        if (strlen($frame->payload) < 8) {
            throw H2Exception::forConnectionError(
                'GOAWAY payload length must be at least 8',
                H2Exception::ERROR_FRAME_SIZE_ERROR
            );
        }

        $lastStreamId = (unpack('N', substr($frame->payload, 0, 4))[1] ?? 0) & H2Frame::MAX_STREAM_ID;
        $this->goAwayReceived = true;
        $this->lastAllowedStreamId = min($this->lastAllowedStreamId, $lastStreamId);

        return $frame;
    }

    private function validateWindowUpdateFrame(H2Frame $frame): H2Frame
    {
        if (strlen($frame->payload) !== 4) {
            throw H2Exception::forConnectionError(
                'WINDOW_UPDATE payload length must be exactly 4',
                H2Exception::ERROR_FRAME_SIZE_ERROR
            );
        }

        $increment = (unpack('N', $frame->payload)[1] ?? 0) & H2Frame::MAX_WINDOW_SIZE;
        if ($increment === 0) {
            throw H2Exception::forConnectionError('WINDOW_UPDATE increment must not be 0');
        }

        if ($frame->streamId > 0 && $this->isStreamClosed($frame->streamId)) {
            throw H2Exception::forStreamError(
                $frame->streamId,
                'WINDOW_UPDATE received for closed stream',
                H2Exception::ERROR_STREAM_CLOSED
            );
        }

        $this->applySendWindowUpdate($frame->streamId, $increment);

        return $frame;
    }

    private function validateRstStreamFrame(H2Frame $frame): H2Frame
    {
        if ($frame->streamId < 1) {
            throw H2Exception::forConnectionError('RST_STREAM frame must use a positive stream ID');
        }

        if (strlen($frame->payload) !== 4) {
            throw H2Exception::forConnectionError(
                'RST_STREAM payload length must be exactly 4',
                H2Exception::ERROR_FRAME_SIZE_ERROR
            );
        }

        $this->markStreamClosed($frame->streamId);

        return $frame;
    }

    private function validateOpenStreamFrame(H2Frame $frame): H2Frame
    {
        $state = $this->streamStates[$frame->streamId] ?? null;
        H2StreamStateMatrix::validateInboundFrame(
            $frame,
            $state,
            $this->isStreamClosed($frame->streamId),
            $frame->streamId > 0
                && $frame->type === H2Frame::TYPE_DATA
                && $state === null
                && !$this->isExistingRequestStream($frame->streamId),
            $frame->streamId > 0
                && $frame->type === H2Frame::TYPE_CONTINUATION
                && $state === null
                && $this->activeHeaderBlockStreamId !== $frame->streamId
                && !$this->scheduler->hasPendingRequestStartFrameForStream($frame->streamId)
        );

        return $frame;
    }

    private function pumpControlFrame(H2Frame $frame, ?int $timeout = null): void
    {
        match ($frame->type) {
            H2Frame::TYPE_SETTINGS => $this->ackSettingsFrameIfNeeded($frame, $timeout),
            H2Frame::TYPE_PING => $this->ackPingFrameIfNeeded($frame, $timeout),
            H2Frame::TYPE_GOAWAY,
            H2Frame::TYPE_WINDOW_UPDATE,
            H2Frame::TYPE_RST_STREAM => null,
            default => throw H2Exception::forConnectionError('Expected HEADERS frame to start header block'),
        };
    }

    private function ackSettingsFrameIfNeeded(H2Frame $frame, ?int $timeout = null): void
    {
        if (($frame->flags & H2Frame::FLAG_ACK) !== 0) {
            return;
        }

        $this->sendSettings(timeout: $timeout, ack: true);
    }

    private function ackPingFrameIfNeeded(H2Frame $frame, ?int $timeout = null): void
    {
        if (($frame->flags & H2Frame::FLAG_ACK) !== 0) {
            return;
        }

        $this->sendPingAck($frame->payload, $timeout);
    }

    private function isGracefulRequestStartTermination(Throwable $exception): bool
    {
        if ($exception instanceof SocketException) {
            return true;
        }

        return $exception instanceof RuntimeException && $exception->getMessage() === 'No frame queued';
    }

    private function nextFrame(?int $timeout = null): H2Frame
    {
        return $this->scheduler->nextPendingFrame($this->readFrame(...), $timeout);
    }

    private function deferFrame(H2Frame $frame): void
    {
        $this->scheduler->defer($frame);
    }

    private function takePendingControlFrame(): ?H2Frame
    {
        return $this->scheduler->takePendingControlFrame();
    }

    private function isSendWindowControlFrame(H2Frame $frame): bool
    {
        return match ($frame->type) {
            H2Frame::TYPE_SETTINGS,
            H2Frame::TYPE_PING,
            H2Frame::TYPE_GOAWAY,
            H2Frame::TYPE_WINDOW_UPDATE,
            H2Frame::TYPE_RST_STREAM => true,
            default => false,
        };
    }

    private function readTrailerHeaderBlock(H2Frame $frame, ?int $timeout = null): H2HeaderBlock
    {
        $trailerBlock = new H2HeaderBlock($frame->streamId);
        $trailerBlock->append($frame);
        $this->activeHeaderBlockStreamId = $frame->streamId;
        try {
            while (!$trailerBlock->isCompleted()) {
                $continuation = $this->nextRelevantFrameForStream($frame->streamId, $timeout);
                $trailerBlock->append($continuation);
            }
        } finally {
            $this->activeHeaderBlockStreamId = null;
        }

        if (($trailerBlock->getFlags() & H2Frame::FLAG_END_STREAM) === 0) {
            throw H2Exception::forConnectionError('Trailing HEADERS frame must end the request stream');
        }

        return $trailerBlock;
    }

    private function nextRelevantFrameForStream(int $streamId, ?int $timeout = null): H2Frame
    {
        return $this->scheduler->nextRelevantFrameForStream($this->readFrame(...), $streamId, $timeout);
    }

    private function takePendingRequestStartFrame(): ?H2Frame
    {
        return $this->scheduler->takePendingRequestStartFrame();
    }

    private function takePendingFrameForStream(int $streamId): ?H2Frame
    {
        return $this->scheduler->takePendingFrameForStream($streamId);
    }

    private function noteRemoteStreamOpened(int $streamId, bool $ended): void
    {
        if ($this->isStreamClosed($streamId)) {
            return;
        }

        $this->streamStates[$streamId] = H2StreamStateMatrix::stateAfterRemoteOpened($ended);
    }

    private function noteRemoteStreamEnded(int $streamId): void
    {
        $nextState = H2StreamStateMatrix::stateAfterRemoteEnded($this->streamStates[$streamId] ?? null);
        if ($nextState === self::STREAM_STATE_CLOSED) {
            $this->markStreamClosed($streamId);

            return;
        }

        if ($nextState !== null) {
            $this->streamStates[$streamId] = $nextState;
        }
    }

    private function noteLocalStreamEnded(int $streamId): void
    {
        $nextState = H2StreamStateMatrix::stateAfterLocalEnded($this->streamStates[$streamId] ?? null);
        if ($nextState === self::STREAM_STATE_CLOSED) {
            $this->markStreamClosed($streamId);

            return;
        }

        if ($nextState !== null) {
            $this->streamStates[$streamId] = $nextState;
        }
    }

    private function applySendWindowUpdate(int $streamId, int $increment): void
    {
        if ($streamId === 0) {
            $nextWindow = $this->connectionSendWindow + $increment;
            if ($nextWindow > H2Frame::MAX_WINDOW_SIZE) {
                throw H2Exception::forConnectionError(
                    'Connection send window exceeds maximum allowed value',
                    H2Exception::ERROR_FLOW_CONTROL_ERROR
                );
            }

            $this->connectionSendWindow = $nextWindow;

            return;
        }

        $currentWindow = $this->streamSendWindows[$streamId] ?? $this->streamSendInitialWindowSize;
        $nextWindow = $currentWindow + $increment;
        if ($nextWindow > H2Frame::MAX_WINDOW_SIZE) {
            throw H2Exception::forStreamError(
                $streamId,
                'Stream send window exceeds maximum allowed value',
                H2Exception::ERROR_FLOW_CONTROL_ERROR
            );
        }

        $this->streamSendWindows[$streamId] = $nextWindow;
    }
}
