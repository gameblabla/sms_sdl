const statusEl = document.getElementById('status');
const canvas = document.getElementById('screen');
const ctx = canvas.getContext('2d');
const drop = document.getElementById('drop');
const file = document.getElementById('file');
let wasm, memory, paused = false, loadedName = '', imageData;
let pad = 0, system = 0, arcade = 0;

const BTN = { UP:1, DOWN:2, LEFT:4, RIGHT:8, B1:16, B2:32, START:64 };
const ARCADE = { COIN1:1, COIN2:2, SERVICE:4, TEST:8, START1:16, START2:32 };

async function boot() {
  const imports = { env: { abort() { throw new Error('wasm abort'); } } };
  const res = await WebAssembly.instantiateStreaming(fetch('smsplusgx.wasm'), imports);
  wasm = res.instance.exports;
  memory = wasm.memory;
  wasm.wasm_core_init();
  requestAnimationFrame(frame);
}

function setStatus(s) { statusEl.textContent = s; }

async function loadFile(f) {
  const buf = new Uint8Array(await f.arrayBuffer());
  const ptr = wasm.wasm_alloc(buf.length);
  new Uint8Array(memory.buffer, ptr, buf.length).set(buf);
  const ok = wasm.wasm_load_rom(ptr, buf.length);
  wasm.wasm_release(ptr);
  if (!ok) { setStatus(`Could not load ${f.name}. For raw clang/WASI builds, use an uncompressed ROM image or a natively supported archive.`); return; }
  loadedName = f.name;
  const key = 'smsplusgx.sram.' + loadedName;
  const saved = localStorage.getItem(key);
  if (saved) loadSram(saved);
  setStatus(`${f.name} loaded. CRC ${wasm.wasm_crc().toString(16).padStart(8,'0').toUpperCase()}`);
}

function loadSram(b64) {
  const bytes = Uint8Array.from(atob(b64), c => c.charCodeAt(0));
  new Uint8Array(memory.buffer, wasm.wasm_sram_ptr(), Math.min(bytes.length, wasm.wasm_sram_size())).set(bytes);
}
function saveSram() {
  if (!loadedName) return;
  const bytes = new Uint8Array(memory.buffer, wasm.wasm_sram_ptr(), wasm.wasm_sram_size());
  let s = ''; for (const b of bytes) s += String.fromCharCode(b);
  localStorage.setItem('smsplusgx.sram.' + loadedName, btoa(s));
  setStatus('SRAM saved to localStorage.');
}

drop.addEventListener('dragover', e => { e.preventDefault(); drop.classList.add('drag'); });
drop.addEventListener('dragleave', () => drop.classList.remove('drag'));
drop.addEventListener('drop', e => { e.preventDefault(); drop.classList.remove('drag'); if (e.dataTransfer.files[0]) loadFile(e.dataTransfer.files[0]); });
file.addEventListener('change', e => { if (e.target.files[0]) loadFile(e.target.files[0]); });
document.getElementById('pause').onclick = () => paused = !paused;
document.getElementById('reset').onclick = () => wasm && wasm.wasm_reset();
document.getElementById('save-sram').onclick = saveSram;
document.getElementById('load-sram').onclick = () => { const s = localStorage.getItem('smsplusgx.sram.' + loadedName); if (s) loadSram(s); };
document.getElementById('scale').oninput = e => { canvas.style.width = `${256 * e.target.value}px`; };
canvas.style.width = '768px';

function key(e, down) {
  const set = (mask, on) => { pad = on ? (pad | mask) : (pad & ~mask); };
  const seta = (mask, on) => { arcade = on ? (arcade | mask) : (arcade & ~mask); };
  switch (e.code) {
    case 'ArrowUp': set(BTN.UP, down); break; case 'ArrowDown': set(BTN.DOWN, down); break;
    case 'ArrowLeft': set(BTN.LEFT, down); break; case 'ArrowRight': set(BTN.RIGHT, down); break;
    case 'KeyZ': set(BTN.B1, down); break; case 'KeyX': set(BTN.B2, down); break;
    case 'Enter': set(BTN.START, down); seta(ARCADE.START1, down); break;
    case 'Digit1': seta(ARCADE.START1, down); break; case 'Digit2': seta(ARCADE.START2, down); break;
    case 'Digit5': seta(ARCADE.COIN1, down); break; case 'Digit6': seta(ARCADE.COIN2, down); break;
    case 'Digit9': seta(ARCADE.SERVICE, down); break; case 'F2': seta(ARCADE.TEST, down); break;
    default: return;
  }
  e.preventDefault();
}
addEventListener('keydown', e => key(e, true));
addEventListener('keyup', e => key(e, false));

function draw() {
  const vx = wasm.wasm_view_x(), vy = wasm.wasm_view_y(), vw = wasm.wasm_view_w(), vh = wasm.wasm_view_h();
  if (canvas.width !== vw || canvas.height !== vh) { canvas.width = vw; canvas.height = vh; imageData = null; }
  imageData ||= ctx.createImageData(vw, vh);
  const ptr = wasm.wasm_framebuffer();
  const pitch = wasm.wasm_pitch();
  const src = new Uint8ClampedArray(memory.buffer, ptr + vy * pitch + vx * 4, vh * pitch);
  const out = imageData.data;
  for (let y=0; y<vh; y++) out.set(src.subarray(y*pitch, y*pitch + vw*4), y*vw*4);
  ctx.putImageData(imageData, 0, 0);
}
function frame() {
  if (wasm && !paused) {
    wasm.wasm_set_pad(0, pad); wasm.wasm_set_system(system); wasm.wasm_set_arcade(arcade);
    wasm.wasm_frame(); draw();
  }
  requestAnimationFrame(frame);
}
boot().catch(e => setStatus(e.message));
