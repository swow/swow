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

/** A real HTTP/2 peer, owned by one PHPT and stopped before its certificates are removed. */
final class Http2Server
{
    /** @var resource|null */
    private $process;

    /** @var array<int, resource> */
    private array $pipes = [];

    private ?string $directory = null;
    public int $port;

    public static function skipIfUnavailable(): void
    {
        $reason = null;
        if (!Swow\Extension::isBuiltWith('curl')) {
            $reason = 'extension must be built with libcurl';
        } elseif (!(curl_version()['features'] & CURL_VERSION_HTTP2)) {
            $reason = 'libcurl must support HTTP/2';
        } elseif (!extension_loaded('openssl')) {
            $reason = 'OpenSSL is required for the HTTP/2 TLS peer';
        } else {
            // A missing optional peer must produce a clean skip; the required CI mode throws below.
            $process = @proc_open([self::node(), '-e', "require('http2')"], [1 => ['pipe', 'w'], 2 => ['pipe', 'w']], $pipes);
            if (!is_resource($process)) {
                $reason = 'Node.js with HTTP/2 support is required';
            } else {
                foreach ($pipes as $pipe) {
                    stream_get_contents($pipe);
                    fclose($pipe);
                }
                if (proc_close($process) !== 0) {
                    $reason = 'Node.js with HTTP/2 support is required';
                }
            }
        }
        if ($reason !== null) {
            if (getenv('TEST_SWOW_HTTP2') === '1') {
                throw new RuntimeException($reason);
            }
            skip($reason);
        }
    }

    private static function node(): string
    {
        return getenv('TEST_SWOW_NODE') ?: 'node';
    }

    public function __construct(public bool $tls = false, int $maxStreams = 100)
    {
        try {
            $arguments = [self::node(), __DIR__ . '/http2-server.cjs', (string) $maxStreams];
            if ($tls) {
                $this->directory = sys_get_temp_dir() . '/swow-http2-' . bin2hex(random_bytes(8));
                Assert::true(mkdir($this->directory));
                $generator = new CertificateGenerator();
                $generator->saveCaCert($this->directory . '/ca.pem');
                $generator->saveNewCertAsFileWithKey('localhost', $this->directory . '/server.pem', subjectAltName: 'DNS:localhost');
                $arguments[] = $this->directory . '/server.pem';
            }
            $this->process = proc_open($arguments, [0 => ['pipe', 'r'], 1 => ['pipe', 'w'], 2 => ['pipe', 'w']], $this->pipes);
            Assert::true(is_resource($this->process), 'Cannot start the HTTP/2 peer');
            stream_set_timeout($this->pipes[1], 10);
            $ready = fgets($this->pipes[1]);
            Assert::notSame($ready, false, 'HTTP/2 peer did not become ready');
            $this->port = json_decode($ready, true, flags: JSON_THROW_ON_ERROR)['port'];
        } catch (Throwable $exception) {
            $this->stop();
            throw $exception;
        }
    }

    /** @return array<int, string|int|bool|array<string>> */
    public function options(string $path): array
    {
        $options = [
            CURLOPT_URL => ($this->tls ? 'https://localhost:' : 'http://127.0.0.1:') . $this->port . $path,
            CURLOPT_PROXY => '',
            CURLOPT_HTTP_VERSION => $this->tls ? CURL_HTTP_VERSION_2TLS : CURL_HTTP_VERSION_2_PRIOR_KNOWLEDGE,
            CURLOPT_PIPEWAIT => true,
            CURLOPT_RETURNTRANSFER => true,
            CURLOPT_TIMEOUT_MS => 5000,
        ];
        if ($this->tls) {
            $options[CURLOPT_CAINFO] = $this->directory . '/ca.pem';
            $options[CURLOPT_RESOLVE] = ['localhost:' . $this->port . ':127.0.0.1'];
            $options[CURLOPT_SSL_VERIFYPEER] = true;
            $options[CURLOPT_SSL_VERIFYHOST] = 2;
        }
        return $options;
    }

    public function stop(): void
    {
        $failure = null;
        if (is_resource($this->process)) {
            // EOF stops even held streams; no HTTP request is needed to shut down a failed test.
            if (isset($this->pipes[0])) {
                fclose($this->pipes[0]);
                unset($this->pipes[0]);
            }
            $deadline = microtime(true) + 5;
            do {
                $status = proc_get_status($this->process);
                if (!$status['running']) {
                    break;
                }
                usleep(1000);
            } while (microtime(true) < $deadline);
            if ($status['running']) {
                proc_terminate($this->process);
            }
            $errors = stream_get_contents($this->pipes[2]);
            foreach ($this->pipes as $pipe) {
                fclose($pipe);
            }
            $this->pipes = [];
            proc_close($this->process);
            $this->process = null;
            if ($status['running'] || $status['exitcode'] !== 0 || $errors !== '') {
                $failure = 'HTTP/2 peer did not exit cleanly: ' . $errors;
            }
        }
        if ($this->directory !== null) {
            foreach (glob($this->directory . '/*') as $file) {
                unlink($file);
            }
            rmdir($this->directory);
            $this->directory = null;
        }
        Assert::null($failure, $failure ?? '');
    }

    public function __destruct()
    {
        $this->stop();
    }
}
