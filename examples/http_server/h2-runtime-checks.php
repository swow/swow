<?php

declare(strict_types=1);

use Swow\Http\Protocol\H2Connection;
use Swow\Http\Protocol\H2Frame;
use Swow\Http\Protocol\H2Headers;
use Swow\Http\Protocol\H2Hpack;
use Swow\Socket;

require dirname(__DIR__, 2) . '/vendor/autoload.php';

const H2_CHECK_SETTINGS_TIMEOUT = 1000;
const H2_CHECK_READ_TIMEOUT = 3000;
const H2_CHECK_INITIAL_WINDOW = 1024;
const H2_CHECK_STREAM_PRIMARY = 1;
const H2_CHECK_STREAM_SECONDARY = 3;
const H2_CHECK_STREAM_TERTIARY = 5;

if ($argc < 3) {
    fwrite(STDERR, "Usage: php examples/http_server/h2-runtime-checks.php <trailers|multistream> <port>\n");
    exit(1);
}

$mode = $argv[1];
$port = (int) $argv[2];

$socket = (new Socket(Socket::TYPE_TCP))->connect('127.0.0.1', $port);
$connection = new H2Connection($socket);
$decoder = new H2Hpack();

sendClientPrefaceAndSettings(
    $socket,
    $connection,
    $mode === 'multistream' ? H2_CHECK_INITIAL_WINDOW : null
);

match ($mode) {
    'trailers' => runTrailerCheck($connection, $decoder, $port),
    'multistream' => runMultiStreamCheck($connection, $port),
    default => throw new InvalidArgumentException('Unsupported mode: ' . $mode),
};

$socket->close();

function runTrailerCheck(H2Connection $connection, H2Hpack $decoder, int $port): void
{
    sendRequestWithTrailers($connection, $port);
    sendResponseTrailerRequest($connection, $port);

    $echoBody = '';
    $responseTrailerBody = '';
    $responseTrailers = [];
    $echoDone = false;
    $trailerDone = false;

    while (!$echoDone || !$trailerDone) {
        $frame = readUsefulFrame($connection);

        if ($frame->streamId === H2_CHECK_STREAM_PRIMARY) {
            if ($frame->type === H2Frame::TYPE_DATA) {
                $echoBody .= $frame->payload;
                if (($frame->flags & H2Frame::FLAG_END_STREAM) !== 0) {
                    $echoDone = true;
                }
            }
            continue;
        }

        if ($frame->streamId !== H2_CHECK_STREAM_SECONDARY) {
            continue;
        }

        if ($frame->type === H2Frame::TYPE_DATA) {
            $responseTrailerBody .= $frame->payload;
            continue;
        }

        if ($frame->type === H2Frame::TYPE_HEADERS) {
            $decoded = $decoder->decode(readHeaderBlock($connection, $frame));
            if (($frame->flags & H2Frame::FLAG_END_STREAM) !== 0) {
                $responseTrailers = H2Headers::createTrailerMap($decoded);
                $trailerDone = true;
            }
        }
    }

    echo 'REQUEST_TRAILER_RESPONSE=' . $echoBody . PHP_EOL;
    echo 'RESPONSE_TRAILER_BODY=' . $responseTrailerBody . PHP_EOL;
    echo 'RESPONSE_TRAILERS=' . json_encode($responseTrailers, JSON_UNESCAPED_UNICODE) . PHP_EOL;
}

function runMultiStreamCheck(H2Connection $connection, int $port): void
{
    $bigDataBytes = 0;
    $completedBeforeWindowUpdate = [];
    $windowUpdateSent = false;
    $completed = [];

    $connection->sendFrame(new H2Frame(
        H2Frame::TYPE_HEADERS,
        H2Frame::FLAG_END_HEADERS | H2Frame::FLAG_END_STREAM,
        H2_CHECK_STREAM_PRIMARY,
        buildRequestHeaderBlock('GET', 'http', '127.0.0.1:' . $port, '/big?size=32768')
    ));
    $connection->sendFrame(new H2Frame(
        H2Frame::TYPE_HEADERS,
        H2Frame::FLAG_END_HEADERS | H2Frame::FLAG_END_STREAM,
        H2_CHECK_STREAM_SECONDARY,
        buildRequestHeaderBlock('GET', 'http', '127.0.0.1:' . $port, '/small?id=3')
    ));
    $connection->sendFrame(new H2Frame(
        H2Frame::TYPE_HEADERS,
        H2Frame::FLAG_END_HEADERS | H2Frame::FLAG_END_STREAM,
        H2_CHECK_STREAM_TERTIARY,
        buildRequestHeaderBlock('GET', 'http', '127.0.0.1:' . $port, '/small?id=5')
    ));

    while (count($completed) < 3) {
        $frame = readUsefulFrame($connection);
        if ($frame->type !== H2Frame::TYPE_DATA) {
            continue;
        }

        if ($frame->streamId === H2_CHECK_STREAM_PRIMARY) {
            $bigDataBytes += strlen($frame->payload);
            if (!$windowUpdateSent && $bigDataBytes >= H2_CHECK_INITIAL_WINDOW) {
                $connection->sendFrame(H2Frame::createWindowUpdate(0, 65535));
                $connection->sendFrame(H2Frame::createWindowUpdate(H2_CHECK_STREAM_PRIMARY, 65535));
                $windowUpdateSent = true;
            }
        }

        if (($frame->flags & H2Frame::FLAG_END_STREAM) !== 0) {
            $completed[$frame->streamId] = true;
            if (!$windowUpdateSent) {
                $completedBeforeWindowUpdate[$frame->streamId] = true;
            }
        }
    }

    ksort($completedBeforeWindowUpdate);
    echo 'COMPLETED_BEFORE_WINDOW_UPDATE=' . json_encode(array_keys($completedBeforeWindowUpdate)) . PHP_EOL;
    echo 'BIG_DATA_BYTES=' . $bigDataBytes . PHP_EOL;
}

function sendClientPrefaceAndSettings(Socket $socket, H2Connection $connection, ?int $initialWindowSize): void
{
    $socket->send(H2Connection::CLIENT_PREFACE);
    $payload = '';
    if ($initialWindowSize !== null) {
        $payload = pack('nN', H2Connection::SETTINGS_INITIAL_WINDOW_SIZE, $initialWindowSize);
    }

    $connection->sendFrame(new H2Frame(H2Frame::TYPE_SETTINGS, 0, 0, $payload));
}

function sendRequestWithTrailers(H2Connection $connection, int $port): void
{
    $headers = buildRequestHeaderBlock('POST', 'http', '127.0.0.1:' . $port, '/echo');
    $trailers = H2Headers::encodeTrailerHeaders(['x-client-trailer' => 'done']);

    $connection->sendFrame(new H2Frame(
        H2Frame::TYPE_HEADERS,
        H2Frame::FLAG_END_HEADERS,
        H2_CHECK_STREAM_PRIMARY,
        $headers
    ));
    $connection->sendFrame(new H2Frame(
        H2Frame::TYPE_DATA,
        0,
        H2_CHECK_STREAM_PRIMARY,
        'payload-body'
    ));
    $connection->sendFrame(new H2Frame(
        H2Frame::TYPE_HEADERS,
        H2Frame::FLAG_END_HEADERS | H2Frame::FLAG_END_STREAM,
        H2_CHECK_STREAM_PRIMARY,
        $trailers
    ));
}

function sendResponseTrailerRequest(H2Connection $connection, int $port): void
{
    $connection->sendFrame(new H2Frame(
        H2Frame::TYPE_HEADERS,
        H2Frame::FLAG_END_HEADERS | H2Frame::FLAG_END_STREAM,
        H2_CHECK_STREAM_SECONDARY,
        buildRequestHeaderBlock('GET', 'http', '127.0.0.1:' . $port, '/trailer')
    ));
}

function readUsefulFrame(H2Connection $connection): H2Frame
{
    while (true) {
        $frame = rawReadFrame($connection->getSocket());
        if ($frame->type === H2Frame::TYPE_SETTINGS) {
            if (($frame->flags & H2Frame::FLAG_ACK) === 0) {
                $connection->sendFrame(new H2Frame(H2Frame::TYPE_SETTINGS, H2Frame::FLAG_ACK, 0, ''));
            }
            continue;
        }
        if ($frame->type === H2Frame::TYPE_PING) {
            if (($frame->flags & H2Frame::FLAG_ACK) === 0) {
                $connection->sendPingAck($frame->payload);
            }
            continue;
        }

        return $frame;
    }
}

function readHeaderBlock(H2Connection $connection, H2Frame $firstFrame): string
{
    $buffer = $firstFrame->payload;
    if (($firstFrame->flags & H2Frame::FLAG_END_HEADERS) !== 0) {
        return $buffer;
    }

    while (true) {
        $frame = readUsefulFrame($connection);
        if ($frame->type !== H2Frame::TYPE_CONTINUATION || $frame->streamId !== $firstFrame->streamId) {
            throw new RuntimeException('Unexpected frame while reading header block');
        }
        $buffer .= $frame->payload;
        if (($frame->flags & H2Frame::FLAG_END_HEADERS) !== 0) {
            return $buffer;
        }
    }
}

function rawReadFrame(Socket $socket): H2Frame
{
    $header = $socket->recvString(9, H2_CHECK_READ_TIMEOUT);
    $length = (ord($header[0]) << 16) | (ord($header[1]) << 8) | ord($header[2]);
    $payload = $length > 0 ? $socket->recvString($length, H2_CHECK_READ_TIMEOUT) : '';

    return H2Frame::fromHeaderAndPayload($header, $payload);
}

function buildRequestHeaderBlock(string $method, string $scheme, string $authority, string $path): string
{
    $methodIndex = match ($method) {
        'GET' => 2,
        'POST' => 3,
        default => throw new InvalidArgumentException('Unsupported method'),
    };
    $schemeIndex = match ($scheme) {
        'http' => 6,
        'https' => 7,
        default => throw new InvalidArgumentException('Unsupported scheme'),
    };

    return encodeIndexedHeader($methodIndex)
        . encodeIndexedHeader($schemeIndex)
        . encodeIndexedNameLiteral(1, $authority)
        . encodeIndexedNameLiteral(4, $path);
}

function encodeIndexedHeader(int $index): string
{
    return encodeInteger($index, 7, 0x80);
}

function encodeIndexedNameLiteral(int $nameIndex, string $value): string
{
    return encodeInteger($nameIndex, 6, 0x40) . encodeStringLiteral($value);
}

function encodeStringLiteral(string $value): string
{
    return encodeInteger(strlen($value), 7, 0) . $value;
}

function encodeInteger(int $value, int $prefixBits, int $prefixMask): string
{
    $maxPrefix = (1 << $prefixBits) - 1;
    if ($value < $maxPrefix) {
        return chr($prefixMask | $value);
    }

    $buffer = chr($prefixMask | $maxPrefix);
    $value -= $maxPrefix;
    while ($value >= 128) {
        $buffer .= chr(($value % 128) + 128);
        $value = intdiv($value, 128);
    }

    return $buffer . chr($value);
}
