// SPDX-License-Identifier: Apache-2.0
// Node 22+: bounded button test using the existing KVM keyboard transport.
const keys = {
  up: 0x52, down: 0x51, left: 0x50, right: 0x4f,
  start: 0x28, back: 0x2a, a: 0x1d, b: 0x1b, x: 0x06, y: 0x19,
  black: 0x05, white: 0x11, lt: 0x14, rt: 0x08, ls: 0x15, rs: 0x17,
  'ls-up': 0x1a, 'ls-down': 0x16, 'ls-left': 0x04, 'ls-right': 0x07,
  'rs-up': 0x0c, 'rs-down': 0x0e, 'rs-left': 0x0d, 'rs-right': 0x0f,
};
const [host, button, duration = '200'] = process.argv.slice(2);
const ms = Number(duration);
if (!host || !(button in keys) || !Number.isInteger(ms) || ms < 100 || ms > 400) {
  console.error('Usage: node tools/xbox-button.mjs http://esp.local BUTTON [100..400 ms]');
  console.error('Buttons:', Object.keys(keys).join(', '));
  process.exit(1);
}
const url = new URL('/ws', host);
if (!['http:', 'https:'].includes(url.protocol)) throw new Error('Use an http(s) device URL');
url.protocol = url.protocol === 'https:' ? 'wss:' : 'ws:';
const ws = new WebSocket(url);
let released = false;
const deadline = setTimeout(() => {
  console.error('Controller connection timed out');
  process.exitCode = 1;
  if (ws.readyState === WebSocket.OPEN) ws.send(new Uint8Array([5]));
  ws.close();
}, 5000);
ws.onopen = () => {
  ws.send(new Uint8Array([3, 0, keys[button], 0, 0, 0, 0, 0]));
  setTimeout(() => {
    if (ws.readyState !== WebSocket.OPEN) return;
    ws.send(new Uint8Array([5]));
    released = true;
    setTimeout(() => ws.close(), 100);
  }, ms);
};
ws.onerror = () => { console.error('WebSocket error'); process.exitCode = 1; };
ws.onclose = () => {
  clearTimeout(deadline);
  if (!released) process.exitCode = 1;
  else console.log(`Sent ${button} for ${ms} ms, then release (verify on the Xbox).`);
};
