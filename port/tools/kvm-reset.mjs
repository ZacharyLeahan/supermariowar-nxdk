// Hold the game's Back+Start recovery chord through the existing ESP KVM.
// Node 22+. Override KVM_URL when testing another explicitly selected KVM.
const supplied = process.argv.slice(2).map(Number);
if (supplied.length && (supplied.length !== 2 || supplied[0] !== 42 || supplied[1] !== 40)) {
  throw new Error('Usage: node kvm-reset.mjs [42 40]');
}
const url = new URL('/ws', process.env.KVM_URL || 'http://192.168.1.169');
if (!['http:', 'https:'].includes(url.protocol)) throw new Error('KVM_URL must use HTTP(S)');
url.protocol = url.protocol === 'https:' ? 'wss:' : 'ws:';
const ws = new WebSocket(url);
const packet = new Uint8Array([3, 0, 42, 40, 0, 0, 0, 0]);
let refresh;
let hold;
let released = false;
const deadline = setTimeout(() => {
  clearInterval(refresh);
  clearTimeout(hold);
  if (ws.readyState === WebSocket.OPEN) ws.send(new Uint8Array([5]));
  process.exitCode = 1;
  ws.close();
}, 10000);
ws.onopen = () => {
  ws.send(new Uint8Array([7]));
  ws.send(packet);
  refresh = setInterval(() => {
    if (ws.readyState === WebSocket.OPEN) ws.send(packet);
  }, 75);
  hold = setTimeout(() => {
    clearInterval(refresh);
    if (ws.readyState !== WebSocket.OPEN) return;
    ws.send(new Uint8Array([5]));
    released = true;
    setTimeout(() => ws.close(), 200);
  }, 5000);
};
ws.onerror = () => { process.exitCode = 1; };
ws.onclose = () => {
  clearInterval(refresh);
  clearTimeout(hold);
  clearTimeout(deadline);
  if (!released) process.exitCode = 1;
};
