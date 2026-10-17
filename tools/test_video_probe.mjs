// SPDX-License-Identifier: Apache-2.0
import assert from 'node:assert/strict';
import { createRequire } from 'node:module';
import { spawn, execFileSync } from 'node:child_process';
import { mkdtempSync, readFileSync, rmSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import { createServer } from 'node:https';
import { createServer as createHttpServer } from 'node:http';
import { once } from 'node:events';
const require = createRequire(new URL('../web/package.json', import.meta.url));
const { WebSocketServer } = require('ws');
for (const earlyClose of [false, true]) {
  const server = new WebSocketServer({ host: '127.0.0.1', port: 0 });
  await once(server, 'listening');
  let acked = false;
  server.on('connection', socket => {
    socket.on('message', data => {
      if (data[0] === 9) {
        assert.equal(data.length, 5);
        assert.equal(data.readUInt32LE(1), 123);
        acked = true;
        if (earlyClose) socket.close();
        return;
      }
      assert.deepEqual([...data], [1]);
      socket.send(Buffer.from([0x85]));
      socket.send(Buffer.from([0x82])); // unrelated short control frame
      const frame = Buffer.alloc(13);
      frame[0] = 0x4b; frame[1] = 1; frame.writeUInt32LE(123, 6);
      socket.send(frame);
    });
  });
  const probe = spawn(process.execPath, [new URL('./video_probe.mjs', import.meta.url).pathname,
    `http://127.0.0.1:${server.address().port}`, '1'], { stdio: ['ignore', 'pipe', 'pipe'] });
  let error = '';
  probe.stderr.on('data', chunk => { error += chunk; });
  const watchdog = setTimeout(() => probe.kill(), 5000);
  const [code] = await once(probe, 'exit');
  clearTimeout(watchdog);
  for (const client of server.clients) client.terminate();
  await new Promise(resolve => server.close(resolve));
  assert.equal(code, earlyClose ? 1 : 0, error);
  assert.equal(acked, true);
  if (earlyClose) assert.match(error, /closed early/);
}
console.log('Video probe: ACK negotiation, short control frames and early disconnect passed');

const temp = mkdtempSync(join(tmpdir(), 'espkvm-probe-tls-'));
try {
  for (const name of ['server', 'untrusted']) {
    execFileSync('openssl', ['req', '-x509', '-newkey', 'rsa:2048', '-nodes',
      '-keyout', join(temp, name + '.key'), '-out', join(temp, name + '.pem'),
      '-days', '1', '-subj', '/CN=localhost', '-addext', 'subjectAltName=IP:127.0.0.1'],
      { stdio: 'ignore' });
  }
  const https = createServer({ key: readFileSync(join(temp, 'server.key')),
    cert: readFileSync(join(temp, 'server.pem')) });
  const server = new WebSocketServer({ server: https });
  server.on('connection', socket => socket.on('message', () => {
    const frame = Buffer.alloc(13); frame[0] = 0x4b; frame[1] = 1;
    socket.send(frame);
  }));
  https.listen(0, '127.0.0.1'); await once(https, 'listening');
  try {
    for (const trusted of [true, false]) {
      const probe = spawn(process.execPath, [new URL('./video_probe.mjs', import.meta.url).pathname,
        `https://127.0.0.1:${https.address().port}`, '1', '',
        join(temp, trusted ? 'server.pem' : 'untrusted.pem')],
        { stdio: ['ignore', 'pipe', 'pipe'] });
      let error = ''; probe.stderr.on('data', chunk => { error += chunk; });
      const watchdog = setTimeout(() => probe.kill(), 5000);
      const [code] = await once(probe, 'exit'); clearTimeout(watchdog);
      assert.equal(code, trusted ? 0 : 1, error);
      if (!trusted) assert.match(error, /certificate/i);
    }
  } finally {
    for (const client of server.clients) client.terminate();
    await new Promise(resolve => server.close(resolve));
    await new Promise(resolve => https.close(resolve));
  }
} finally { rmSync(temp, { recursive: true, force: true }); }
console.log('Video probe: pinned CA accepted and unrelated CA rejected');

const stalled = createHttpServer();
const pending = new Set();
stalled.on('connection', socket => { pending.add(socket); socket.on('close', () => pending.delete(socket)); });
stalled.on('upgrade', () => {}); // accepts TCP but never finishes the upgrade
stalled.listen(0, '127.0.0.1'); await once(stalled, 'listening');
try {
  const probe = spawn(process.execPath, [new URL('./video_probe.mjs', import.meta.url).pathname,
    `http://127.0.0.1:${stalled.address().port}`, '1'], { stdio: ['ignore', 'pipe', 'pipe'] });
  let error = ''; probe.stderr.on('data', chunk => { error += chunk; });
  const watchdog = setTimeout(() => probe.kill(), 8000);
  const [code] = await once(probe, 'exit'); clearTimeout(watchdog);
  assert.equal(code, 1, error); assert.match(error, /timed out/i);
} finally {
  for (const socket of pending) socket.destroy();
  await new Promise(resolve => stalled.close(resolve));
}
console.log('Video probe: stalled handshake fails within its connection deadline');
