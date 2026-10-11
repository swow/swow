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

/** Per-stream response storage; yielding callbacks must never mix sibling transfer data. */
final class Http2Transfer
{
    public CurlHandle $handle;
    public string $body = '';
    public string $headers = '';
    public int $writes = 0;

    public function __construct(Http2Server $server, string $path, int $yieldMicroseconds = 0)
    {
        $this->handle = curl_init();
        Assert::true(curl_setopt_array($this->handle, $server->options($path)));
        Assert::true(curl_setopt($this->handle, CURLOPT_WRITEFUNCTION, function (CurlHandle $handle, string $data) use ($yieldMicroseconds): int {
            Assert::same($handle, $this->handle);
            if ($yieldMicroseconds > 0) {
                usleep($yieldMicroseconds);
            }
            $this->body .= $data;
            $this->writes++;
            return strlen($data);
        }));
        Assert::true(curl_setopt($this->handle, CURLOPT_HEADERFUNCTION, function (CurlHandle $handle, string $data): int {
            Assert::same($handle, $this->handle);
            $this->headers .= $data;
            return strlen($data);
        }));
    }

    public static function multi(): CurlMultiHandle
    {
        $multi = curl_multi_init();
        Assert::true(curl_multi_setopt($multi, CURLMOPT_PIPELINING, CURLPIPE_MULTIPLEX));
        Assert::true(curl_multi_setopt($multi, CURLMOPT_MAX_HOST_CONNECTIONS, 1));
        return $multi;
    }

    public function add(CurlMultiHandle $multi): void
    {
        Assert::same(curl_multi_add_handle($multi, $this->handle), CURLM_OK);
    }

    public function remove(CurlMultiHandle $multi): void
    {
        Assert::same(curl_multi_remove_handle($multi, $this->handle), CURLM_OK);
    }

    public static function until(CurlMultiHandle $multi, Closure $condition): void
    {
        $deadline = microtime(true) + 10;
        while (true) {
            Assert::same(curl_multi_exec($multi, $running), CURLM_OK);
            if ($condition()) {
                return;
            }
            Assert::greaterThan($running, 0, 'Transfers finished before the expected protocol event');
            Assert::greaterThanEq(curl_multi_select($multi, 0.1), -1);
            Assert::lessThan(microtime(true), $deadline, 'Expected HTTP/2 event did not arrive');
        }
    }

    /** @return list<array{msg: int, result: int, handle: CurlHandle}> */
    public static function run(CurlMultiHandle $multi): array
    {
        $deadline = microtime(true) + 10;
        do {
            Assert::same(curl_multi_exec($multi, $running), CURLM_OK);
            if ($running > 0) {
                Assert::greaterThanEq(curl_multi_select($multi, 0.1), -1);
                Assert::lessThan(microtime(true), $deadline, 'HTTP/2 multi did not finish');
            }
        } while ($running > 0);
        $messages = [];
        while (($message = curl_multi_info_read($multi)) !== false) {
            Assert::same($message['msg'], CURLMSG_DONE);
            $messages[] = $message;
        }
        return $messages;
    }

    /** @param list<array{msg: int, result: int, handle: CurlHandle}> $messages */
    public static function assertSuccessful(array $messages, int $count): void
    {
        Assert::same(count($messages), $count);
        $handles = [];
        foreach ($messages as $message) {
            $id = spl_object_id($message['handle']);
            Assert::false(isset($handles[$id]), 'A transfer produced duplicate completion messages');
            $handles[$id] = true;
            Assert::same($message['result'], CURLE_OK, curl_error($message['handle']));
            Assert::same(curl_getinfo($message['handle'], CURLINFO_HTTP_VERSION), CURL_HTTP_VERSION_2_0);
            Assert::same(curl_getinfo($message['handle'], CURLINFO_RESPONSE_CODE), 200);
        }
    }

    /** @return array{session: int, stream: int, port: int, alpn: string, path: string, method: string, body: string, batch: int} */
    public function response(): array
    {
        return json_decode($this->body, true, flags: JSON_THROW_ON_ERROR);
    }

    public function clear(): void
    {
        $this->body = $this->headers = '';
        $this->writes = 0;
    }
}
