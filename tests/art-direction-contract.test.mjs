import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import test from 'node:test';

import { ART_DIRECTION_SPEC } from '../src/art-direction.js';

function pngDimensions(buffer) {
  assert.equal(buffer.subarray(1, 4).toString('ascii'), 'PNG');
  return { width: buffer.readUInt32BE(16), height: buffer.readUInt32BE(20) };
}

test('all adjacent zones retain the required two-unit overlap', () => {
  assert.equal(ART_DIRECTION_SPEC.zones.length, 6);
  for (let index = 0; index < ART_DIRECTION_SPEC.zones.length - 1; index += 1) {
    const left = ART_DIRECTION_SPEC.zones[index];
    const right = ART_DIRECTION_SPEC.zones[index + 1];
    assert.ok(left.maxX - right.minX >= ART_DIRECTION_SPEC.overlapWidth,
      `${left.id}/${right.id} overlap is too narrow`);
  }
  for (const seam of ART_DIRECTION_SPEC.seams) {
    assert.equal(seam.maxX - seam.minX, ART_DIRECTION_SPEC.overlapWidth);
    assert.ok(seam.x > seam.minX && seam.x < seam.maxX);
  }
});

test('world scale matches the live character and camera contract', () => {
  assert.equal(ART_DIRECTION_SPEC.cameraHeight, 18);
  assert.equal(ART_DIRECTION_SPEC.characterHeight, 1.72);
  assert.equal(ART_DIRECTION_SPEC.pixelsPerUnit, 64);
  assert.equal(ART_DIRECTION_SPEC.world.surfaceGroundY, -4.55);
  assert.equal(ART_DIRECTION_SPEC.world.labGroundY, -22.4);
});

test('production panoramas preserve the dimensions used by their world transforms', async () => {
  const expectations = new Map([
    ['public/art/gate/surface-facility-shared.png', [1932, 814]],
    ['public/art/elevator/elevator-lab-far-2047-master.png', [1448, 1086]],
    ['public/art/elevator/elevator-lab-mid-2147-master.png', [1448, 1086]],
    ['public/art/lab/lab-2047-longscroll.png', [1905, 826]],
    ['public/art/lab/lab-2147-longscroll.png', [1906, 825]],
    ['public/art/boss-arena/excavator-hall-past.png', [2172, 724]],
    ['public/art/boss-arena/excavator-hall-present.png', [2172, 724]],
  ]);
  for (const [file, expected] of expectations) {
    const dimensions = pngDimensions(await readFile(new URL(`../${file}`, import.meta.url)));
    assert.deepEqual([dimensions.width, dimensions.height], expected, file);
  }
});

test('background art stays enabled while editor gameplay components remain active', async () => {
  const state = JSON.parse(await readFile(
    new URL('../public/editor/editor-state.json', import.meta.url),
    'utf8',
  ));
  assert.equal(state.sceneArt.enabled, true);
  assert.equal(state.sceneArt.backgroundRevision, 'surface-facility-2026-08-10');
  assert.ok(state.assets.length >= 10);
  assert.ok(state.assets.every(asset => asset.visible !== false));
  assert.ok(state.assets.some(asset => asset.physicsType === 'pushable'));
  assert.ok(state.assets.some(asset => asset.animation?.type === 'rotate'));
});
