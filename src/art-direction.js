const WORLD = Object.freeze({
  surfaceGroundY: -4.55,
  labGroundY: -22.4,
  minX: -17,
  maxX: 116,
});

export const ART_DIRECTION_SPEC = Object.freeze({
  cameraHeight: 18,
  pixelsPerUnit: 64,
  characterHeight: 1.72,
  overlapWidth: 2,
  world: WORLD,
  zones: [
    { id: 'Z01', name: '地上闸门', minX: -17, maxX: 14, groundY: -4.55 },
    { id: 'Z02', name: '矿井电梯', minX: 12, maxX: 21.5, groundY: -4.55, targetGroundY: -22.4 },
    { id: 'Z03', name: '实验室入口', minX: 19.5, maxX: 34, groundY: -22.4 },
    { id: 'Z04', name: '实验室主体', minX: 32, maxX: 58, groundY: -22.4 },
    { id: 'Z05', name: '防爆门', minX: 56, maxX: 66, groundY: -22.4 },
    { id: 'Z06', name: 'Boss 大厅', minX: 64, maxX: 116, groundY: -22.4 },
  ],
  parallax: [
    { id: 'far-rock', factorX: .18, factorY: .35 },
    { id: 'mid-facility', factorX: .52, factorY: .72 },
    { id: 'near-structure', factorX: .88, factorY: .94 },
  ],
  seams: [
    { id: 'S01', x: 13, minX: 12, maxX: 14, anchor: '03 号闸门右承重柱' },
    { id: 'S12', x: 20.5, minX: 19.5, maxX: 21.5, anchor: '井底层站门框' },
    { id: 'S23', x: 32, minX: 31, maxX: 33, anchor: '样本库第 05 跨柱' },
    { id: 'S34', x: 56, minX: 55, maxX: 57, anchor: '检修间双立柱' },
    { id: 'S45', x: 64, minX: 63, maxX: 65, anchor: '防爆门后安全线' },
  ],
});

const PALETTES = Object.freeze({
  past: {
    void: '#160c0d', rock: '#2a1514', rockEdge: '#47251e', rockLine: '#683728',
    metal: '#3c211c', metalEdge: '#9a5738', metalDark: '#211315', trim: '#c97743',
    floor: '#5d3025', cable: '#79432e', glow: '#67d5df', glowHot: '#b6f9f5', haze: '#327d84',
  },
  present: {
    void: '#050d11', rock: '#10181a', rockEdge: '#28373a', rockLine: '#3c5357',
    metal: '#18272a', metalEdge: '#486b70', metalDark: '#0c1519', trim: '#71462f',
    floor: '#26383a', cable: '#314a4e', glow: '#56cedd', glowHot: '#c0ffff', haze: '#25636e',
  },
});

function makeMaterial(THREE, color, opacity = 1) {
  const result = new THREE.MeshBasicMaterial({ color, transparent: opacity < 1, opacity, depthWrite: opacity >= 1 });
  result.userData.baseOpacity = opacity;
  return result;
}

function rect(THREE, group, width, height, color, x, y, z = 0, opacity = 1) {
  const mesh = new THREE.Mesh(new THREE.PlaneGeometry(width, height), makeMaterial(THREE, color, opacity));
  mesh.position.set(x, y, z);
  group.add(mesh);
  return mesh;
}

function disc(THREE, group, radius, color, x, y, z = 0, opacity = 1, segments = 48) {
  const mesh = new THREE.Mesh(new THREE.CircleGeometry(radius, segments), makeMaterial(THREE, color, opacity));
  mesh.position.set(x, y, z);
  group.add(mesh);
  return mesh;
}

function polygon(THREE, group, points, color, z = 0, opacity = 1) {
  const shape = new THREE.Shape();
  shape.moveTo(points[0][0], points[0][1]);
  for (let index = 1; index < points.length; index += 1) shape.lineTo(points[index][0], points[index][1]);
  shape.closePath();
  const mesh = new THREE.Mesh(new THREE.ShapeGeometry(shape), makeMaterial(THREE, color, opacity));
  mesh.position.z = z;
  group.add(mesh);
  return mesh;
}

function beam(THREE, group, x1, y1, x2, y2, width, color, z = 0, opacity = 1) {
  const dx = x2 - x1;
  const dy = y2 - y1;
  const mesh = rect(THREE, group, Math.hypot(dx, dy), width, color, (x1 + x2) * 0.5, (y1 + y2) * 0.5, z, opacity);
  mesh.rotation.z = Math.atan2(dy, dx);
  return mesh;
}

function imagePlane(THREE, group, options) {
  const texture = new THREE.TextureLoader().load(options.url);
  texture.colorSpace = THREE.SRGBColorSpace;
  texture.minFilter = THREE.LinearMipmapLinearFilter;
  texture.magFilter = THREE.LinearFilter;
  texture.generateMipmaps = true;
  if (options.repeat) texture.repeat.set(options.repeat[0], options.repeat[1]);
  if (options.offset) texture.offset.set(options.offset[0], options.offset[1]);
  const material = new THREE.MeshBasicMaterial({
    map: texture,
    color: options.color || '#ffffff',
    transparent: true,
    opacity: options.opacity ?? 1,
    depthWrite: false,
    toneMapped: false,
  });
  material.userData.baseOpacity = options.opacity ?? 1;
  material.userData.forceTransparent = true;
  const mesh = new THREE.Mesh(new THREE.PlaneGeometry(options.width, options.height), material);
  mesh.name = options.name;
  mesh.position.set(options.x, options.y, options.z || 0);
  mesh.userData.baseX = options.x;
  mesh.userData.parallaxFactor = options.parallaxFactor || 0;
  mesh.userData.parallaxAnchorX = options.parallaxAnchorX || 0;
  mesh.renderOrder = options.renderOrder || 0;
  group.add(mesh);
  return mesh;
}

function addCrystalCluster(THREE, group, palette, x, y, scale, ruined, z) {
  disc(THREE, group, scale * 1.25, palette.haze, x, y, z - .05, ruined ? .08 : .13, 64);
  const shards = [
    { dx: 0, dy: .15, w: .35, h: 1.35, r: 0 },
    { dx: -.42, dy: -.12, w: .28, h: .82, r: -.3 },
    { dx: .43, dy: -.18, w: .3, h: .92, r: .28 },
  ];
  for (const shard of shards) {
    polygon(THREE, group, [
      [x + shard.dx, y + shard.dy + shard.h * scale * .55],
      [x + shard.dx - shard.w * scale * .5, y + shard.dy],
      [x + shard.dx, y + shard.dy - shard.h * scale * .45],
      [x + shard.dx + shard.w * scale * .5, y + shard.dy],
    ], palette.glow, z, ruined ? .44 : .78);
  }
  disc(THREE, group, scale * .12, palette.glowHot, x, y + scale * .1, z + .02, ruined ? .46 : .92, 32);
}

function addRockStrata(THREE, group, palette, y, z, ruined) {
  const minX = -78;
  const maxX = 198;
  rect(THREE, group, maxX - minX, 15.5, palette.rock, (minX + maxX) * .5, y, z, .98);
  for (let row = 0; row < 9; row += 1) {
    const baseY = y - 6.1 + row * 1.45;
    const phase = row * 1.77;
    for (let x = minX - 2; x < maxX + 2; x += 3.8) {
      const jitter = Math.sin(x * .31 + phase) * .32;
      const stone = rect(THREE, group, 3.45, .84, row % 2 ? palette.rockEdge : palette.rockLine, x + (row % 2) * 1.7, baseY + jitter, z + .02, ruined ? .16 : .22);
      stone.rotation.z = Math.sin(x * .23 + phase) * .035;
    }
  }
  for (const [x, offset, scale] of [[-20, 4.1, .72], [8, -2.6, .58], [36, 2.8, .82], [69, -1.8, .62], [101, 3.1, .95], [138, -2.2, .68]]) {
    addCrystalCluster(THREE, group, palette, x, y + offset, scale, ruined, z + .08);
  }
}

function addFacilityBays(THREE, group, palette, baseY, minX, maxX, bay, height, ruined, z) {
  rect(THREE, group, maxX - minX, .34, palette.metalDark, (minX + maxX) * .5, baseY + height, z, .96);
  rect(THREE, group, maxX - minX, .13, palette.metalEdge, (minX + maxX) * .5, baseY + height - .18, z + .02, ruined ? .32 : .6);
  let bayIndex = 0;
  for (let x = minX; x <= maxX + .01; x += bay) {
    if (ruined && bayIndex % 7 === 3) {
      beam(THREE, group, x, baseY + .2, x + .6, baseY + height - .3, .18, palette.metalEdge, z, .34);
    } else {
      rect(THREE, group, .26, height, palette.metal, x, baseY + height * .5, z, ruined ? .55 : .88);
      rect(THREE, group, .08, height - .25, palette.metalEdge, x - .08, baseY + height * .5, z + .02, ruined ? .28 : .62);
      beam(THREE, group, x + .12, baseY + .3, x + bay - .12, baseY + height - .28, .09, palette.metalEdge, z + .01, ruined ? .22 : .48);
    }
    bayIndex += 1;
  }
}

function addPipeRun(THREE, group, palette, minX, maxX, y, ruined, z) {
  for (let row = 0; row < 3; row += 1) {
    const offset = row * .24;
    const end = ruined && row === 1 ? maxX - 8.5 : maxX;
    rect(THREE, group, end - minX, .075, row === 2 ? palette.trim : palette.cable, (minX + end) * .5, y - offset, z, ruined ? .3 : .62);
    for (let x = minX + 2.5; x < end; x += 8) rect(THREE, group, .18, .56, palette.metalEdge, x, y - .24, z + .02, ruined ? .35 : .66);
  }
}

function buildFar(THREE, group, palette, ruined) {
  addRockStrata(THREE, group, palette, -1.0, 0, ruined);
  addRockStrata(THREE, group, palette, -18.1, .01, ruined);
  rect(THREE, group, 12, 37, palette.void, 18.25, -12.2, .1, .72);
  for (let y = -30; y <= 8; y += 3.3) {
    rect(THREE, group, 11.2, .22, palette.rockEdge, 18.25, y, .12, ruined ? .18 : .28);
  }
}

function buildMid(THREE, group, palette, ruined) {
  addFacilityBays(THREE, group, palette, WORLD.surfaceGroundY, -28, 38, 5.5, 10.8, ruined, 0);
  addFacilityBays(THREE, group, palette, WORLD.labGroundY, -10, 145, 7.5, 8.7, ruined, 0);
  addPipeRun(THREE, group, palette, -30, 39, WORLD.surfaceGroundY + 8.2, ruined, .04);
  addPipeRun(THREE, group, palette, -12, 146, WORLD.labGroundY + 7.25, ruined, .04);

  rect(THREE, group, 7.2, 34.5, palette.metalDark, 18.25, -12.0, .02, .84);
  for (const x of [15.15, 21.35]) rect(THREE, group, .34, 34.5, palette.metalEdge, x, -12.0, .05, ruined ? .42 : .8);
  for (let y = -29; y < 5; y += 2.2) beam(THREE, group, 15.4, y, 21.1, y + 1.5, .1, palette.metalEdge, .05, ruined ? .28 : .56);

  for (const x of [25, 36, 48, 58, 72, 89, 106]) {
    const radius = x === 89 ? 2.7 : 1.15;
    disc(THREE, group, radius, palette.metalDark, x, WORLD.labGroundY + 4.6, .08, .94, 64);
    disc(THREE, group, radius * .82, palette.metalEdge, x, WORLD.labGroundY + 4.6, .09, ruined ? .24 : .4, 64);
    disc(THREE, group, radius * .68, palette.void, x, WORLD.labGroundY + 4.6, .1, .98, 64);
    if (x === 36 || x === 89) addCrystalCluster(THREE, group, palette, x, WORLD.labGroundY + 4.6, x === 89 ? 1.15 : .68, ruined, .12);
  }
}

function addFloorFascia(THREE, group, palette, minX, maxX, y, ruined, z) {
  rect(THREE, group, maxX - minX, .46, palette.floor, (minX + maxX) * .5, y - .28, z, .98);
  rect(THREE, group, maxX - minX, .09, palette.metalEdge, (minX + maxX) * .5, y + .02, z + .02, ruined ? .42 : .82);
  for (let x = minX + .9; x < maxX; x += 2.4) {
    const plate = rect(THREE, group, 1.55, .2, x % 4.8 < 1 ? palette.trim : palette.metal, x, y - .34, z + .03, ruined ? .44 : .75);
    plate.rotation.z = ruined && Math.sin(x) > .75 ? .07 : 0;
  }
}

function buildNear(THREE, group, palette, ruined) {
  addFloorFascia(THREE, group, palette, -38, 42, WORLD.surfaceGroundY, ruined, 0);
  addFloorFascia(THREE, group, palette, -18, 152, WORLD.labGroundY, ruined, 0);
  for (const [minX, maxX, y] of [[-26, 40, 6.3], [-8, 149, WORLD.labGroundY + 8.2]]) {
    for (let x = minX; x < maxX; x += 5.8) {
      const sag = ruined && Math.round(x) % 3 === 0 ? 1.15 : .65;
      beam(THREE, group, x, y, x + 2.9, y - sag, .08, palette.cable, .04, ruined ? .38 : .68);
      beam(THREE, group, x + 2.9, y - sag, x + 5.8, y, .08, palette.cable, .04, ruined ? .38 : .68);
    }
  }
  for (const x of [12, 14, 19.5, 21.5, 31, 33, 55, 57, 63, 65]) {
    rect(THREE, group, .08, 8.4, palette.glow, x, WORLD.labGroundY + 4.2, .08, .06);
  }
}

function buildGateProduction(THREE, group) {
  imagePlane(THREE, group, {
    name: 'gate-surface-terrain-v3',
    url: '/art/gate/surface-terrain-only-v3.png',
    width: 38.2,
    height: 16.1,
    x: -1,
    y: .26,
    z: 0,
    opacity: 1,
    color: '#ffffff',
    parallaxFactor: 0,
  });
}

function buildElevatorProduction(THREE, group, palette, ruined) {
  const era = ruined ? '2147' : '2047';
  // Preserve the task-2 authored world mapping exactly. The surface floor,
  // shaft centre and lab-floor stop are baked against these bounds.
  const width = 43.04 - 8.25;
  const height = 1.34 - (-24.76);
  const x = (8.25 + 43.04) * .5;
  const y = (-24.76 + 1.34) * .5;
  imagePlane(THREE, group, {
    name: `elevator-${era}-far`,
    url: `/art/elevator/elevator-lab-far-${era}-master.png`,
    width, height, x, y, z: 0,
    opacity: 1,
    color: '#ffffff',
    parallaxFactor: .015,
    parallaxAnchorX: 18.25,
  });
  imagePlane(THREE, group, {
    name: `elevator-${era}-mid`,
    url: `/art/elevator/elevator-lab-mid-${era}-master.png`,
    width, height, x, y, z: .08,
    opacity: 1,
    color: '#ffffff',
    renderOrder: 600,
  });
}

function buildLabProduction(THREE, group, palette, ruined) {
  const era = ruined ? '2147' : '2047';
  const source = ruined
    ? { width: 1906, height: 825, floorPixel: 650 }
    : { width: 1905, height: 826, floorPixel: 650 };
  const left = 18.1;
  const width = 46;
  const height = width * source.height / source.width;
  const topY = WORLD.labGroundY + height * source.floorPixel / source.height;
  imagePlane(THREE, group, {
    name: `lab-${era}-longscroll`,
    url: `/art/lab/lab-${era}-longscroll.png`,
    width,
    height,
    x: left + width * .5,
    y: topY - height * .5,
    z: 0,
    opacity: 1,
    color: '#ffffff',
    parallaxFactor: 0,
    parallaxAnchorX: 41.1,
  });
}

function buildBossProduction(THREE, group, palette, ruined) {
  imagePlane(THREE, group, {
    name: ruined ? 'boss-panorama-present' : 'boss-panorama-past',
    url: ruined
      ? '/art/boss-arena/excavator-hall-present.png'
      : '/art/boss-arena/excavator-hall-past.png',
    width: 55.5,
    height: 18.5,
    x: 88.5,
    y: WORLD.labGroundY + 9.25,
    z: 0,
    opacity: 1,
    color: '#ffffff',
    parallaxFactor: .008,
    parallaxAnchorX: 89,
  });
}

function setOpacity(group, amount) {
  group.traverse(object => {
    if (!object.material) return;
    const materials = Array.isArray(object.material) ? object.material : [object.material];
    for (const material of materials) {
      const baseOpacity = material.userData.baseOpacity ?? 1;
      material.opacity = baseOpacity * amount;
      const forceTransparent = material.userData.forceTransparent === true;
      material.transparent = forceTransparent || material.opacity < .999;
      material.depthWrite = !forceTransparent && material.opacity >= .999;
    }
  });
  group.visible = amount > .004;
}

export function createMineParallaxArt({ THREE, scene }) {
  const definitions = [
    // Surface production art is terrain-only. Every identifiable facility
    // model is a transparent editor asset, so visual motion and physics share
    // one runtime driver instead of leaving a duplicate baked into the plate.
    { name: 'gate-production', factorX: 1, factorY: 1, z: .8, presentZOffset: .01, build: buildGateProduction },
    { name: 'elevator-production', factorX: 1, factorY: 1, z: -3.4, presentZOffset: 1, build: buildElevatorProduction },
    // The lab owns the 60.75-64.1 overlap; the boss panorama starts behind it.
    { name: 'lab-production', factorX: 1, factorY: 1, z: .8, presentZOffset: .01, build: buildLabProduction },
    { name: 'boss-production', factorX: 1, factorY: 1, z: -3.38, presentZOffset: 1, build: buildBossProduction },
  ];
  const records = [];

  for (const definition of definitions) {
    for (const era of ['past', 'present']) {
      const group = new THREE.Group();
      group.name = `art-${definition.name}-${era}`;
      group.position.z = definition.z + (era === 'present' ? definition.presentZOffset || 0 : 0);
      definition.build(THREE, group, PALETTES[era], era === 'present');
      scene.add(group);
      records.push({ ...definition, era, group });
    }
  }

  return {
    update(cameraX, cameraY, eraBlend) {
      const blend = Math.min(1, Math.max(0, eraBlend));
      for (const record of records) {
        record.group.position.x = cameraX * (1 - record.factorX);
        record.group.position.y = cameraY * (1 - record.factorY);
        for (const mesh of record.group.children) {
          if (!mesh.userData?.parallaxFactor) continue;
          mesh.position.x = mesh.userData.baseX
            + (cameraX - mesh.userData.parallaxAnchorX) * mesh.userData.parallaxFactor;
        }
        setOpacity(record.group, record.era === 'past' ? 1 - blend : blend);
      }
    },
    dispose() {
      for (const record of records) {
        record.group.traverse(object => {
          object.geometry?.dispose?.();
          object.material?.dispose?.();
        });
        scene.remove(record.group);
      }
    },
  };
}
