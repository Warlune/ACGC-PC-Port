import { open, stat } from 'node:fs/promises';
import { createHash } from 'node:crypto';
import { createReadStream } from 'node:fs';

// Read the GameCube CISO block map without converting or changing the image.
const path = process.argv[2];
if (!path) throw new Error('Usage: node tools/inspect-disc.mjs <disc.ciso|iso|gcm>');
const file = await open(path, 'r');
try {
  const header = Buffer.alloc(0x8000);
  const { bytesRead } = await file.read(header, 0, header.length, 0);
  if (bytesRead !== header.length) throw new Error('Truncated image header');
  const ciso = header.toString('ascii', 0, 4) === 'CISO';
  const blockSize = ciso ? header.readUInt32LE(4) : 0;
  if (ciso && (!blockSize || (blockSize & (blockSize - 1)))) throw new Error('Invalid CISO block size');
  const physical = [];
  if (ciso) {
    let count = 0;
    for (const entry of header.subarray(8)) physical.push(entry ? count++ : -1);
    if (0x8000 + count * blockSize > (await stat(path)).size) throw new Error('Truncated CISO data');
  }
  async function read(offset, size) {
    const out = Buffer.alloc(size);
    for (let done = 0; done < size;) {
      const pos = offset + done;
      const block = ciso ? Math.floor(pos / blockSize) : 0;
      const within = ciso ? pos % blockSize : pos;
      const length = ciso ? Math.min(size - done, blockSize - within) : size - done;
      if (!ciso || physical[block] >= 0) {
        const source = ciso ? 0x8000 + physical[block] * blockSize + within : within;
        const result = await file.read(out, done, length, source);
        if (result.bytesRead !== length) throw new Error('Truncated disc data');
      }
      done += length;
    }
    return out;
  }
  const disc = await read(0, 0x440);
  if (disc.readUInt32BE(0x1c) !== 0xc2339f3d) throw new Error('GameCube disc magic does not match');
  const hash = createHash('sha256');
  for await (const chunk of createReadStream(path)) hash.update(chunk);
  const result = {
    file: path, format: ciso ? 'GameCube CISO' : 'ISO/GCM',
    sha256: hash.digest('hex'), gameId: disc.toString('ascii', 0, 6),
    revision: disc[7], discNumber: disc[6],
    title: disc.toString('ascii', 0x20, 0x400).split('\0')[0],
    mainDolOffset: disc.readUInt32BE(0x420),
    fstOffset: disc.readUInt32BE(0x424), fstSize: disc.readUInt32BE(0x428),
    supportedBySelectedPort: disc.toString('ascii', 0, 6) === 'GAFE01' && disc[7] === 0,
  };
  console.log(JSON.stringify(result, null, 2));
} finally { await file.close(); }
