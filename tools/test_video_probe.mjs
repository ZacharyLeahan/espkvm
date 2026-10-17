// SPDX-License-Identifier: Apache-2.0
import assert from 'node:assert/strict';
import { createRequire } from 'node:module';
import { spawn } from 'node:child_process';
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
