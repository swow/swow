<?php

declare(strict_types=1);

use Psr\Http\Message\ServerRequestInterface;
use Swow\Coroutine;
use Swow\Psr7\Psr7;
use Swow\Psr7\Server\EventDriverConnectionHandler;
use Swow\Psr7\Server\H2ResponseEnvelope;
use Swow\Psr7\Server\H2ServerConnection;
use Swow\Psr7\Server\Server;
use Swow\Psr7\Server\ServerConnection;
use Swow\SocketException;

require dirname(__DIR__, 2) . '/vendor/autoload.php';
require dirname(__DIR__, 2) . '/ext/tests/include/bootstrap.php';

const H2_RUNTIME_BIG_SIZE = 262144;
const H2_RUNTIME_MAX_CONNECTIONS = 64;
const H2_RUNTIME_CERTIFICATE = __DIR__ . '/certs/localhost.pem';
const H2_RUNTIME_CERTIFICATE_KEY = __DIR__ . '/certs/localhost-key.pem';

$mode = $argv[1] ?? 'tls';
$maxConnections = isset($argv[2]) ? (int) $argv[2] : H2_RUNTIME_MAX_CONNECTIONS;
$host = $argv[3] ?? 'localhost';
$x509Base = '/tmp/swow_h2_runtime_x509';

$server = new Server();
$server->bind($host, 9764)->listen();
$port = $server->getSockPort();

$tlsOptions = null;
if ($mode === 'tls') {
    $paths = null;
    if (!is_file(H2_RUNTIME_CERTIFICATE) || !is_file(H2_RUNTIME_CERTIFICATE_KEY)) {
        $paths = testX509Paths($x509Base);
    }
    $tlsOptions = [
        'certificate' => $paths['localhost']['cert'] ?? H2_RUNTIME_CERTIFICATE,
        'certificate_key' => $paths['localhost']['key'] ?? H2_RUNTIME_CERTIFICATE_KEY,
        'verify_peer' => false,
        'alpn_protocols' => 'h2,http/1.1',
    ];
}

$handler = new EventDriverConnectionHandler(
    connectionHandler: static function (ServerConnection|H2ServerConnection $connection): void {
        if (!method_exists($connection, 'getNegotiatedAlpnProtocol')) {
            return;
        }

        $protocol = $connection->getNegotiatedAlpnProtocol();
        echo 'CONNECTION_ALPN=' . ($protocol ?? 'null') . PHP_EOL;
    },
    requestHandler: static function (ServerConnection|H2ServerConnection $connection, ServerRequestInterface $request): array|H2ResponseEnvelope {
        $path = $request->getUri()->getPath();
        $query = [];
        parse_str($request->getUri()->getQuery(), $query);
        $requestBody = (string) $request->getBody();
        $trailers = $connection instanceof H2ServerConnection
            ? H2ServerConnection::getRequestTrailers($request)
            : [];

        if ($path === '/health') {
            return [
                'status' => 200,
                'headers' => ['content-type' => 'application/json'],
                'body' => json_encode([
                    'path' => $path,
                    'protocol' => $request->getProtocolVersion(),
                    'method' => $request->getMethod(),
                ], JSON_UNESCAPED_UNICODE),
            ];
        }

        if ($path === '/echo') {
            return [
                'status' => 200,
                'headers' => ['content-type' => 'application/json'],
                'body' => json_encode([
                    'path' => $path,
                    'protocol' => $request->getProtocolVersion(),
                    'method' => $request->getMethod(),
                    'body' => $requestBody,
                    'trailers' => $trailers,
                ], JSON_UNESCAPED_UNICODE),
            ];
        }

        if ($path === '/small') {
            $id = (string) ($query['id'] ?? 'none');
            return [
                'status' => 200,
                'headers' => [
                    'content-type' => 'text/plain',
                    'x-small-id' => $id,
                ],
                'body' => 'small:' . $id,
            ];
        }

        if ($path === '/big') {
            $size = isset($query['size']) ? max(1, (int) $query['size']) : H2_RUNTIME_BIG_SIZE;
            return [
                'status' => 200,
                'headers' => [
                    'content-type' => 'text/plain',
                    'x-big-size' => (string) $size,
                ],
                'body' => str_repeat('B', $size),
            ];
        }

        if ($path === '/trailer') {
            return H2ResponseEnvelope::from(
                Psr7::createResponse(
                    code: 200,
                    headers: ['content-type' => 'text/plain'],
                    body: 'with-trailer'
                ),
                ['x-trailer-status' => 'done']
            );
        }

        if ($path === '/goaway') {
            if ($connection instanceof H2ServerConnection) {
                $connection->getConnection()->goAway(
                    lastStreamId: $connection->getConnection()->getLastProcessedStreamId()
                );
            }

            return [
                'status' => 200,
                'headers' => ['content-type' => 'text/plain'],
                'body' => 'goaway',
            ];
        }

        return [
            'status' => 404,
            'headers' => ['content-type' => 'text/plain'],
            'body' => 'not-found',
        ];
    },
    upgradeHandler: null,
    messageHandler: null,
    closeHandler: static function (): void {
        echo "CONNECTION_CLOSED\n";
    },
    exceptionHandler: static function (ServerConnection|H2ServerConnection $connection, Throwable $throwable): void {
        echo 'EXCEPTION=' . $throwable::class . ':' . $throwable->getMessage() . PHP_EOL;
    },
);

echo 'MODE=' . $mode . PHP_EOL;
echo 'HOST=' . $host . PHP_EOL;
echo 'PORT=' . $port . PHP_EOL;

$accepted = 0;
while ($accepted < $maxConnections) {
    try {
        $connection = $server->acceptConnection();
        Coroutine::run(static function () use ($connection, $handler, $mode, $tlsOptions): void {
            if ($mode === 'tls') {
                $connection->enableCrypto($tlsOptions);
            }

            $handler->handle($connection);
        });
        $accepted++;
    } catch (SocketException $exception) {
        echo 'SERVER_EXCEPTION=' . $exception->getMessage() . PHP_EOL;
        break;
    }
}

$server->close();
