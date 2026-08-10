import assert from 'node:assert/strict';
import { createHash } from 'node:crypto';
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
    ['public/art/gate/surface-terrain-only-v3.png', [1930, 815]],
    ['public/art/gate/surface-models-master-v3.png', [1928, 816]],
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
  assert.equal(state.sceneArt.backgroundRevision, 'surface-terrain-only-model-split-2026-08-10');
  assert.equal(state.sceneArt.pastImage, '/art/gate/surface-terrain-only-v3.png');
  assert.equal(state.sceneArt.presentImage, '/art/gate/surface-terrain-only-v3.png');
  assert.ok(state.assets.length >= 11);
  assert.ok(state.assets.every(asset => asset.visible !== false));
  assert.ok(state.assets.some(asset => asset.physicsType === 'pushable'));
  assert.ok(state.assets.some(asset => asset.animation?.type === 'rotate'));
  const gateLeaf = state.assets.find(asset => asset.id === 'scene-gate-leaf');
  assert.deepEqual(gateLeaf.driver, { id: 'surface-gate-lift', axis: 'y', distance: 5.2 });
  assert.equal(gateLeaf.width, 1.55);
  assert.equal(gateLeaf.height, 5.15);
});

test('every active editor component has visually distinct era textures', async () => {
  const state = JSON.parse(await readFile(
    new URL('../public/editor/editor-state.json', import.meta.url),
    'utf8',
  ));

  for (const asset of state.assets) {
    const version = asset.versions.at(-1);
    assert.notEqual(version.pastImage, version.presentImage, `${asset.id} reuses one URL for both eras`);

    const [past, present] = await Promise.all([
      readFile(new URL(`../public${version.pastImage}`, import.meta.url)),
      readFile(new URL(`../public${version.presentImage}`, import.meta.url)),
    ]);
    const digest = buffer => createHash('sha256').update(buffer).digest('hex');
    assert.notEqual(digest(past), digest(present), `${asset.id} has byte-identical era textures`);
  }
});
