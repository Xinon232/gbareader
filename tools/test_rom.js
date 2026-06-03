// GBA ROM verification using gbats (Node.js GBA emulator).
// Cited from /home/hlm/.hermes/skills/gaming/gba-rom-build-test/scripts/.
//
// Why gbats and not mgba: gbats doesn't need a BIOS file, so it works
// in a headless container. mgba 0.10.2 from apt requires a real BIOS
// and refuses to boot without it.
//
// gbats quirk: the constructor takes (bios, updateMethod), not a ROM.
// The bundled BIOS is at lib/es6/assets/bios.js (base64 encoded). It
// is loaded by default. We call setRom(rom) separately to load the cart.

const g = require('gbats');
const fs = require('fs');

const ROM_PATH = process.argv[2] || 'build/vocab.gba';
const MAX_STEPS = 10000;

const rom = fs.readFileSync(ROM_PATH);

// Patch byte at 0xb2 (gbats also wants this)
rom[0xb2] = 0x96;

// First arg is BIOS (null = use bundled), then update method.
// We pass the rom array as second path via setRom later.
const emu = new g.GameBoyAdvance(null, 'manual');

// Load the ROM (sets PC to the BIOS entry point)
emu.setRom(rom.buffer.slice(rom.byteOffset, rom.byteOffset + rom.byteLength));

// Reset CPU to ROM start (0x08000000)
// gbats quirk: resetCPU() may not work in some versions. Write directly
// to the gprs array if the reset doesn't take.
emu.cpu.resetCPU(0x08000000);
if (emu.cpu.gprs) {
    emu.cpu.gprs[15] = 0x08000000;  // PC
    emu.cpu.gprs[13] = 0x03007F00;  // SP (IWRAM top, per our _start.s)
}

const pc0 = emu.cpu.PC;
let illegalOpcode = false;
let lastPC = pc0;
let stuckCount = 0;
let framesRun = 0;

for (let i = 0; i < MAX_STEPS; i++) {
    try {
        // gbats: doStep is aliased to waitFrame which is a no-op.
        // We need step() which calls cpu.step() in a loop until a vblank.
        emu.step();
        framesRun++;
    } catch (e) {
        illegalOpcode = true;
        console.log('Opcode error at step', i, ':', e.message);
        break;
    }
    const pc = emu.cpu.PC;
    if (pc === lastPC) {
        stuckCount++;
        if (stuckCount > 50) {
            // Might be in a spin loop. That's fine — we just want PC to advance first.
            break;
        }
    } else {
        stuckCount = 0;
        lastPC = pc;
    }
}

const pc1 = emu.cpu.PC;

console.log('=== ROM verification ===');
console.log('File:', ROM_PATH);
console.log('Size:', rom.length, 'bytes (expected 262144)');
console.log('PC initial: 0x' + pc0.toString(16));
console.log('PC after', framesRun, 'frames: 0x' + pc1.toString(16));
console.log('PC changed:', pc0 !== pc1 ? 'YES ✓' : 'NO ✗ (stuck)');
console.log('Illegal opcode:', illegalOpcode ? 'YES ✗' : 'NO ✓');
console.log('Vectors at 0xC0-0xDF:', rom.slice(0xC0, 0xC8).toString('hex') + '...');
console.log('Byte at 0xb2: 0x' + rom[0xb2].toString(16));

if (!illegalOpcode && pc0 !== pc1) {
    console.log('=== VERDICT: ROM boots and executes code ===');
    process.exit(0);
} else {
    console.log('=== VERDICT: ROM did NOT boot cleanly ===');
    process.exit(1);
}
