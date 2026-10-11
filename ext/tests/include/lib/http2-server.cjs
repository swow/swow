'use strict';

/**
 * This file is part of Swow.
 * https://github.com/swow/swow
 * See the distributed LICENSE file for copyright and licensing details.
 */

// Use Node's HTTP/2 implementation so tests exercise real framing, TLS and flow control.
const http2 = require('http2');
const fs = require('fs');
const { NGHTTP2_CANCEL, NGHTTP2_NO_ERROR } = http2.constants;
const settings = { maxConcurrentStreams: Number(process.argv[2]) };
const pem = process.argv[3] ? fs.readFileSync(process.argv[3]) : null;
const server = pem
    ? http2.createSecureServer({ key: pem, cert: pem, allowHTTP1: false, settings })
    : http2.createServer({ settings });

/** @type {Map<import('http2').ServerHttp2Session, number>} */
const sessions = new Map();
/** @type {Map<string, Array<() => void>>} */
const barriers = new Map();
let nextSession = 0;
let stopping = false;

function stop() {
    if (stopping) return;
    stopping = true;
    clearTimeout(watchdog);
    process.stdin.destroy();
    for (const session of sessions.keys()) session.destroy();
    server.close();
}

// A failed or killed PHPT must not leave its local peer behind.
const watchdog = setTimeout(() => {
    process.exitCode = 1;
    stop();
}, 25000);
process.stdin.resume();
process.stdin.on('end', stop);
process.on('SIGTERM', stop);
server.on('error', (error) => {
    console.error(error);
    process.exitCode = 1;
    stop();
});
server.on('session', (session) => {
    sessions.set(session, ++nextSession);
    session.on('error', () => {});
    session.on('close', () => sessions.delete(session));
});

server.on('stream', (stream, headers) => {
    const session = stream.session;
    const path = String(headers[':path']);
    const url = new URL(path, 'http://localhost');
    /** @type {Buffer[]} */
    const chunks = [];
    stream.on('error', () => {});
    stream.on('data', (chunk) => chunks.push(chunk));
    stream.on('end', () => {
        const metadata = {
            session: sessions.get(session), stream: stream.id,
            port: session.socket.remotePort, alpn: session.socket.alpnProtocol || 'h2c',
            path, method: headers[':method'], body: Buffer.concat(chunks).toString('base64'), batch: 1,
        };
        // Expose the negotiated protocol before gating application data. The barrier
        // still requires concurrent streams on one session, without also withholding headers.
        stream.respond({
            ':status': 200,
            'content-type': url.pathname === '/data' ? 'application/octet-stream' : 'application/json',
            'x-session': String(metadata.session), 'x-stream': String(metadata.stream),
        }, { waitForTrailers: url.pathname === '/data' });
        const respond = (batch = 1) => {
            if (stream.destroyed) return;
            if (url.pathname === '/reset') {
                stream.close(NGHTTP2_CANCEL);
                return;
            }
            metadata.batch = batch;
            if (url.pathname === '/hold') {
                // Intentionally incomplete: the client controls cancellation or its transfer deadline.
                return;
            }
            if (url.pathname === '/data') {
                const size = Number(url.searchParams.get('size'));
                const unit = Buffer.from(url.searchParams.get('id') + '\0\xff', 'latin1');
                const data = Buffer.alloc(size);
                for (let offset = 0; offset < size; offset += unit.length) {
                    unit.copy(data, offset, 0, Math.min(unit.length, size - offset));
                }
                stream.on('wantTrailers', () => stream.sendTrailers({ 'x-body-sha256': require('crypto').createHash('sha256').update(data).digest('hex') }));
                stream.end(data);
                return;
            }
            if (url.pathname === '/goaway') session.goaway(NGHTTP2_NO_ERROR, stream.id);
            stream.end(JSON.stringify(metadata));
        };
        const count = Number(url.searchParams.get('count') || 1);
        if (count > 1) {
            // No response body is sent until N streams arrive on the same connection.
            const key = `${sessions.get(session)}:${url.searchParams.get('group')}`;
            const pending = barriers.get(key) || [];
            pending.push(() => respond(count));
            barriers.set(key, pending);
            if (pending.length === count) {
                barriers.delete(key);
                for (const release of pending.reverse()) release();
            }
        } else {
            respond();
        }
    });
});

server.listen(0, '127.0.0.1', () => {
    process.stdout.write(JSON.stringify({ port: server.address().port }) + '\n');
});
