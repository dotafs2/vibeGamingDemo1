import * as THREE from './vendor/three.module.js';

const canvas = document.querySelector('#game');
const eraLabel = document.querySelector('#era-label');
const objective = document.querySelector('#objective');
const toast = document.querySelector('#toast');
const timeFlash = document.querySelector('#time-flash');
const interaction = document.querySelector('#interaction');
const eventCrate = document.querySelector('#event-crate');
const eventPlate = document.querySelector('#event-plate');
const eventGate = document.querySelector('#event-gate');
const eventElevator = document.querySelector('#event-elevator');
const eventReactor = document.querySelector('#event-reactor');
const eventFuture = document.querySelector('#event-future');
const inventoryPanel = document.querySelector('#inventory-panel');
const handwheelSlot = document.querySelector('#handwheel-slot');
const inventoryItemName = document.querySelector('#inventory-item-name');
const inventoryItemStatus = document.querySelector('#inventory-item-status');
const doorStatus = document.querySelector('#door-status');
const doorPower = document.querySelector('#door-power');
const doorLock = document.querySelector('#door-lock');
const hotbarSword = document.querySelector('#hotbar-sword');
const hotbarSpear = document.querySelector('#hotbar-spear');
const hotbarWheel = document.querySelector('#hotbar-wheel');
const playerHealthFill = document.querySelector('#player-health-fill');
const playerHealthCopy = document.querySelector('#player-health-copy');
const bossHud = document.querySelector('#boss-hud');
const bossHealthFill = document.querySelector('#boss-health-fill');
const bossHealthCopy = document.querySelector('#boss-health-copy');

const scene = new THREE.Scene();
const camera = new THREE.OrthographicCamera(-16, 16, 9, -9, 0.1, 100);
camera.position.z = 20;

const renderer = new THREE.WebGLRenderer({ canvas, antialias: true, alpha: false });
renderer.setPixelRatio(Math.min(devicePixelRatio, 2));
renderer.outputColorSpace = THREE.SRGBColorSpace;

const clock = new THREE.Clock();
const keys = new Set();
const groundY = -4.55;
const labGroundY = -22.4;
const elevatorX = 18.25;
const labEntranceX = 21.7;
const reactorControlX = 47.2;
const futureBarrierX = 57.4;
const bossTriggerX = 65.0;

const state = {
  era: 1,
  eraTarget: 1,
  pulse: 0,
  lastToggle: -10,
  gateLift: 0,
  exitReached: false,
  elevatorY: groundY,
  elevatorTargetY: labGroundY,
  elevatorRiding: false,
  elevatorAtBottom: false,
  bossAwake: false,
  selectedWeapon: 'sword',
  attackTimer: 0,
  attackDuration: 0,
  attackCooldown: 0,
  attackHit: false,
  boss: {
    maxHealth: 320,
    health: 320,
    x: 75,
    vx: 0,
    decisionTimer: 1.4,
    chargeTimer: 0,
    projectileTimer: 1.2,
    hitFlash: 0,
    defeated: false,
  },
  cameraX: 0,
  cameraY: 0,
  inventoryOpen: false,
  inventory: {
    handwheel: false,
  },
  history: {
    wheelCollected: false,
    wheelCrossed: false,
    wheelInstalled: false,
    gateOpened: false,
    elevatorUsed: false,
    experimentShutdown: false,
    futureCleared: false,
  },
};

const player = {
  x: -10.4,
  y: groundY + .86,
  vx: 0,
  vy: 0,
  halfW: .38,
  halfH: .86,
  speed: 6.2,
  jumpSpeed: 10.4,
  grounded: true,
  facing: 1,
  walkPhase: 0,
  walkBlend: 0,
  maxHealth: 100,
  health: 100,
  hurtCooldown: 0,
};

const vertexShader = /* glsl */`
  varying vec2 vUv;
  void main() {
    vUv = uv;
    gl_Position = projectionMatrix * modelViewMatrix * vec4(position, 1.0);
  }
`;

const fragmentShader = /* glsl */`
  uniform float uEra;
  uniform float uTime;
  uniform float uPulse;
  varying vec2 vUv;

  float lineGrid(vec2 p, float size) {
    vec2 g = abs(fract(p * size - .5) - .5) / max(fwidth(p * size), .001);
    return 1.0 - min(min(g.x, g.y), 1.0);
  }

  void main() {
    vec3 pastTop = vec3(.12, .022, .024);
    vec3 pastBottom = vec3(.035, .012, .017);
    vec3 presentTop = vec3(.018, .070, .082);
    vec3 presentBottom = vec3(.014, .026, .031);
    vec3 past = mix(pastBottom, pastTop, smoothstep(.0, 1.0, vUv.y));
    vec3 present = mix(presentBottom, presentTop, smoothstep(.0, 1.0, vUv.y));
    vec3 color = mix(past, present, uEra);

    float grid = lineGrid(vUv + vec2(uTime * .0014, 0.), 32.0) * .018;
    float horizon = .025 / max(abs(vUv.y - .41), .024);
    vec3 eraGlow = mix(vec3(.95, .21, .055), vec3(.12, .70, .82), uEra);
    color += eraGlow * (grid + horizon * .05 + uPulse * .08);

    float vignette = smoothstep(.9, .25, distance(vUv, vec2(.5)));
    color *= .72 + vignette * .36;
    gl_FragColor = vec4(color, 1.0);
  }
`;

const backgroundMaterial = new THREE.ShaderMaterial({
  vertexShader,
  fragmentShader,
  uniforms: {
    uEra: { value: 1 },
    uTime: { value: 0 },
    uPulse: { value: 0 },
  },
  depthWrite: false,
});

const background = new THREE.Mesh(new THREE.PlaneGeometry(140, 70), backgroundMaterial);
background.position.set(34, -10, -12);
scene.add(background);

function material(color, opacity = 1) {
  const value = new THREE.MeshBasicMaterial({ color, transparent: true, opacity, depthWrite: opacity >= 1 });
  value.userData.baseOpacity = opacity;
  return value;
}

function lineMaterial(color, opacity = 1) {
  const value = new THREE.LineBasicMaterial({ color, transparent: true, opacity });
  value.userData.baseOpacity = opacity;
  return value;
}

function rectangle(group, width, height, color, x, y, z = 0, opacity = 1) {
  const mesh = new THREE.Mesh(new THREE.PlaneGeometry(width, height), material(color, opacity));
  mesh.position.set(x, y, z);
  group.add(mesh);
  return mesh;
}

function segment(group, x1, y1, x2, y2, color, opacity = 1, z = .1) {
  const geometry = new THREE.BufferGeometry().setFromPoints([
    new THREE.Vector3(x1, y1, z),
    new THREE.Vector3(x2, y2, z),
  ]);
  const object = new THREE.Line(geometry, lineMaterial(color, opacity));
  group.add(object);
  return object;
}

function disc(group, radius, color, x, y, z = 0, opacity = 1, segments = 48) {
  const mesh = new THREE.Mesh(new THREE.CircleGeometry(radius, segments), material(color, opacity));
  mesh.position.set(x, y, z);
  group.add(mesh);
  return mesh;
}

function ring(group, outerRadius, innerRadius, color, x, y, z = 0, opacity = 1, segments = 48) {
  const mesh = new THREE.Mesh(
    new THREE.RingGeometry(innerRadius, outerRadius, segments),
    material(color, opacity),
  );
  mesh.position.set(x, y, z);
  group.add(mesh);
  return mesh;
}

function polygon(group, points, color, x = 0, y = 0, z = 0, opacity = 1) {
  const shape = new THREE.Shape();
  shape.moveTo(points[0][0], points[0][1]);
  for (let index = 1; index < points.length; index++) shape.lineTo(points[index][0], points[index][1]);
  shape.closePath();
  const mesh = new THREE.Mesh(new THREE.ShapeGeometry(shape), material(color, opacity));
  mesh.position.set(x, y, z);
  group.add(mesh);
  return mesh;
}

function setLayerOpacity(group, amount) {
  group.traverse(object => {
    if (!object.material) return;
    const materials = Array.isArray(object.material) ? object.material : [object.material];
    for (const item of materials) {
      const base = item.userData.baseOpacity ?? 1;
      item.opacity = base * amount;
      item.transparent = true;
    }
  });
}

const pastLayer = new THREE.Group();
const presentLayer = new THREE.Group();
pastLayer.position.z = -2;
presentLayer.position.z = -1;
scene.add(pastLayer, presentLayer);

const pastLampMaterials = [];
const pastPowerNodeMaterials = [];
const pastDoorPowerMaterials = [];
const pastMovingOre = [];
let pastVentFan = null;
let pastElevatorCar = null;
let pastElevatorCable = null;
let pastReactorCore = null;
let pastShutdownLever = null;
let modernOvergrowth = null;
let modernInertCore = null;
let modernBoss = null;
let pastBoss = null;
let bossBattleBarrier = null;
const bossCoreMaterials = [];
const bossProjectiles = [];

function buildMineLandmarks(group, palette, ruined) {
  const fade = ruined ? .72 : .96;

  // 同一座地下03号矿石转运站：破碎区、通风维修区、深井闸门区。
  rectangle(group, 8.3, 5.0, palette.hall, -9.25, -1.4, -2.6, fade);
  rectangle(group, 5.7, 4.35, palette.workshop, -2.15, -1.73, -2.6, fade);
  rectangle(group, 3.55, 6.35, palette.gatehouse, 8.35, -1.38, -2.7, ruined ? .7 : .94);
  rectangle(group, 5.7, 4.7, palette.shaft, 13.0, -1.75, -3.1, ruined ? .62 : .86);

  // 岩石顶板是两个时代不变的地理轮廓，避免场景看起来像露天厂房。
  polygon(group, [
    [-16, 9], [16, 9], [16, 6.8], [13.8, 6.4], [11.4, 6.75], [8.2, 6.3],
    [5.5, 6.65], [2.9, 6.25], [.2, 6.7], [-2.7, 6.2], [-5.8, 6.55],
    [-8.5, 6.05], [-11.2, 6.5], [-13.9, 5.95], [-16, 6.35],
  ], palette.rock, 0, 0, -4.2, ruined ? .72 : .9);
  for (const [x, y, radius] of [[-13.7, 5.95, .5], [-6.1, 6.05, .42], [1.1, 6.2, .52], [10.9, 6.2, .45]]) {
    disc(group, radius, palette.oreVein, x, y, -4, ruined ? .14 : .25, 18);
  }

  // 跨时代坐标完全一致的承重柱与顶梁。
  for (const x of [-13.0, -9.8, -6.0, -4.8, .55, 6.55, 10.15, 12.3, 14.6]) {
    const support = rectangle(group, .2, 5.15, palette.structure, x, -1.05, -1.95, ruined ? .34 : .78);
    if (ruined && (x === -9.8 || x === .55)) support.rotation.z = x < 0 ? -.095 : .075;
  }
  for (const [x, width] of [[-9.5, 7.2], [-2.1, 5.55], [8.35, 3.6]]) {
    rectangle(group, width, .22, palette.trim, x, 1.48, -1.9, ruined ? .3 : .78);
  }
  segment(group, 10.2, .55, 15.7, .55, palette.trim, ruined ? .28 : .68, -2.0);
  for (const x of [11.0, 13.0, 15.0]) {
    segment(group, x, groundY + .1, x, .55, palette.structure, ruined ? .28 : .62, -1.85);
    segment(group, x, .55, x + .55, 1.05, palette.structure, ruined ? .24 : .5, -1.85);
  }
  for (const x of [-12.8, -6.25, -4.55, .3, 6.75, 9.9]) {
    segment(group, x, 1.35, x + .72, .68, palette.structure, ruined ? .25 : .55, -1.75);
  }

  // 左侧：投料斗、双辊破碎机和矿石输送带。
  const hopper = polygon(group, [[-1.4, .7], [1.4, .7], [.72, -.65], [-.72, -.65]], palette.machine, -10.4, .03, -1.15, fade);
  if (ruined) hopper.rotation.z = -.055;
  rectangle(group, 2.15, .24, palette.trim, -10.4, .82, -1.0, ruined ? .42 : .9);
  segment(group, -11.25, -.63, -10.75, -1.32, palette.structure, ruined ? .35 : .78, -1.0);
  segment(group, -9.55, -.63, -10.05, -1.32, palette.structure, ruined ? .35 : .78, -1.0);
  rectangle(group, 2.3, 1.42, palette.machineDark, -10.4, -2.03, -1.0, fade);
  const crusherLeft = disc(group, .51, palette.machine, -10.92, -1.92, -.82, ruined ? .6 : .98, 28);
  const crusherRight = disc(group, .51, palette.machine, -9.88, -1.92, -.82, ruined ? .54 : .98, 28);
  for (let index = 0; index < 8; index++) {
    const angle = index * Math.PI / 4;
    segment(group, -10.92, -1.92, -10.92 + Math.cos(angle) * .45, -1.92 + Math.sin(angle) * .45, palette.trim, ruined ? .25 : .58, -.7);
    segment(group, -9.88, -1.92, -9.88 + Math.cos(angle) * .45, -1.92 + Math.sin(angle) * .45, palette.trim, ruined ? .22 : .58, -.7);
  }
  crusherLeft.rotation.z = ruined ? .14 : 0;
  crusherRight.rotation.z = ruined ? -.08 : 0;

  if (!ruined) {
    rectangle(group, 6.15, .25, palette.belt, -8.3, -3.16, -.94, .9);
    segment(group, -11.4, -3.02, -5.2, -3.02, palette.trim, .88, -.78);
    segment(group, -11.4, -3.34, -5.2, -3.34, palette.structure, .72, -.78);
    for (let x = -11.15; x <= -5.45; x += .58) disc(group, .12, palette.trim, x, -3.18, -.7, .68, 16);
    for (let index = 0; index < 7; index++) {
      const ore = polygon(group, [[-.22, -.12], [-.1, .17], [.17, .2], [.25, -.08]], palette.ore, -10.8 + index * .78, -2.82, -.55, .96);
      ore.userData.baseX = ore.position.x;
      pastMovingOre.push(ore);
    }
  } else {
    const beltA = rectangle(group, 2.5, .25, palette.belt, -10.1, -3.15, -.94, .5);
    const beltB = rectangle(group, 2.45, .25, palette.belt, -6.65, -3.35, -.94, .38);
    beltA.rotation.z = -.04;
    beltB.rotation.z = .11;
    segment(group, -8.8, -3.11, -8.15, -3.55, palette.trim, .42, -.7);
    segment(group, -7.95, -3.52, -7.2, -3.34, palette.trim, .35, -.7);
    for (let index = 0; index < 8; index++) {
      polygon(group, [[-.24, -.13], [-.08, .2], [.2, .15], [.27, -.12]], palette.ore, -11.1 + index * .82, -3.64 - (index % 2) * .16, -.55, .48);
    }
  }

  // 中央：大型通风机与维修台，是地下矿场的第二个识别地标。
  rectangle(group, 3.05, 2.8, palette.machineDark, -2.45, -.92, -1.35, ruined ? .48 : .9);
  disc(group, 1.1, palette.trim, -2.45, -.78, -1.06, ruined ? .4 : .85, 48);
  disc(group, .88, palette.fanDark, -2.45, -.78, -.96, .96, 48);
  const fan = new THREE.Group();
  fan.position.set(-2.45, -.78, -.82);
  for (let index = 0; index < 4; index++) {
    const blade = polygon(fan, [[0, .08], [.66, .2], [.78, .48], [.2, .52]], palette.fan, 0, 0, 0, ruined ? .48 : .9);
    blade.rotation.z = index * Math.PI / 2;
  }
  disc(fan, .18, palette.trim, 0, 0, .05, .95, 20);
  if (ruined) fan.rotation.z = .28;
  else pastVentFan = fan;
  group.add(fan);
  rectangle(group, 2.8, .3, palette.trim, 1.9, -2.75, -1.0, ruined ? .42 : .85);
  rectangle(group, .18, 1.05, palette.structure, .75, -3.28, -1.05, ruined ? .38 : .75);
  rectangle(group, .18, 1.05, palette.structure, 3.05, -3.28, -1.05, ruined ? .32 : .75);
  for (let index = 0; index < 4; index++) {
    rectangle(group, .24, .38 + index * .1, palette.tool, 1.15 + index * .5, -2.3, -.82, ruined ? .27 : .72);
  }

  // 连通三个功能区的矿车轨道，过去完整、现代原位断裂。
  const railEnd = ruined ? 5.8 : 7.05;
  if (!ruined) {
    segment(group, -14.6, groundY + .38, railEnd, groundY + .38, palette.rail, .9, -.35);
    segment(group, -14.6, groundY + .16, railEnd, groundY + .16, palette.railDark, .8, -.36);
  } else {
    segment(group, -14.6, groundY + .38, -1.0, groundY + .38, palette.rail, .5, -.35);
    segment(group, -.55, groundY + .23, 2.65, groundY + .38, palette.rail, .38, -.35);
    segment(group, 3.05, groundY + .38, railEnd, groundY + .58, palette.rail, .3, -.35);
  }
  for (let x = -14.2; x <= railEnd; x += .72) {
    const sleeper = rectangle(group, .46, .11, palette.sleeper, x, groundY + .08, -.42, ruined ? .48 : .85);
    if (ruined && Math.round((x + 14.2) / .72) % 6 === 0) sleeper.rotation.z = .28;
  }

  const cart = new THREE.Group();
  cart.position.set(-5.25, groundY + .72, -.18);
  polygon(cart, [[-1.05, .5], [1.05, .5], [.78, -.38], [-.78, -.38]], palette.cart, 0, 0, 0, ruined ? .58 : .96);
  segment(cart, -1.03, .5, 1.03, .5, palette.trim, ruined ? .42 : .86, .06);
  disc(cart, .26, palette.wheel, -.63, -.46, .08, .96, 24);
  disc(cart, .26, palette.wheel, .63, -.46, .08, .96, 24);
  if (!ruined) {
    for (const [x, y, radius] of [[-.58, .58, .32], [-.05, .66, .38], [.52, .58, .3]]) {
      disc(cart, radius, palette.ore, x, y, -.02, .96, 9);
    }
  } else {
    cart.rotation.z = -.32;
    cart.position.y -= .18;
  }
  group.add(cart);

  // 供电管线与灯具把2047年的正常设备一路连接到通电闸门。
  if (ruined) {
    segment(group, -13.0, 2.02, -7.4, 2.02, palette.conduit, .24);
    segment(group, -6.95, 1.95, -2.1, 1.48, palette.conduit, .2);
    segment(group, -1.65, 1.4, 2.35, 1.7, palette.conduit, .18);
    segment(group, 2.85, 1.58, 5.4, 1.1, palette.conduit, .17);
  } else {
    segment(group, -13.0, 2.02, 5.35, 2.02, palette.conduit, .82);
    segment(group, 5.35, 2.02, 7.35, 3.55, palette.conduit, .72);
  }
  const lampXs = [-11.6, -7.3, -2.5, 1.85, 5.05];
  for (const x of lampXs) {
    segment(group, x, 1.98, x, 1.55, palette.structure, ruined ? .26 : .62, -.65);
    const lamp = disc(group, .17, ruined ? palette.deadLamp : palette.lamp, x, 1.42, -.58, ruined ? .3 : .98, 20);
    if (!ruined) pastLampMaterials.push(lamp.material);
  }
  if (!ruined) {
    for (let index = 0; index < 13; index++) {
      const node = disc(group, .052, palette.lamp, -11.8 + index * 1.4, 2.02, -.52, .55, 16);
      pastPowerNodeMaterials.push(node.material);
    }
  }

  // 现代损坏只叠加在相同机器上，不改变地图坐标。
  if (ruined) {
    const fallenDuct = rectangle(group, 3.0, .24, palette.conduit, 2.25, -.12, -.55, .3);
    fallenDuct.rotation.z = -.18;
    for (let index = 0; index < 9; index++) {
      const chunk = polygon(group, [[-.25, -.14], [-.08, .2], [.22, .13], [.28, -.16]], palette.debris, -12.6 + index * 1.72, groundY + .28 + (index % 3) * .1, -.08, .6);
      chunk.rotation.z = -.35 + index * .12;
    }
    segment(group, -12.15, 1.42, -11.3, .55, palette.crack, .35, -.35);
    segment(group, -11.3, .55, -11.75, -.35, palette.crack, .26, -.35);
    segment(group, -.25, 1.4, -.9, .28, palette.crack, .3, -.35);
    segment(group, -.9, .28, -.45, -.65, palette.crack, .24, -.35);
  }
}

function buildPastScene() {
  buildMineLandmarks(pastLayer, {
    hall: '#35191a', workshop: '#43201e', gatehouse: '#321819', shaft: '#241316', tower: '#3d1b1a',
    structure: '#a25735', trim: '#d17645', tank: '#6f1f1d', conduit: '#f08b4b',
    lamp: '#ffb15d', deadLamp: '#7d4d37', debris: '#5e2d22', crack: '#bd6845',
    rock: '#271013', oreVein: '#ff8a43', machine: '#8f4a2f', machineDark: '#51261f',
    belt: '#2b1718', ore: '#e47a3e', fan: '#c56b3e', fanDark: '#241416', tool: '#e2a169',
    rail: '#e09358', railDark: '#6d3a2c', sleeper: '#704027', cart: '#9f5232', wheel: '#211619',
  }, false);
}

function buildPresentScene() {
  buildMineLandmarks(presentLayer, {
    hall: '#132d34', workshop: '#142e34', gatehouse: '#10282f', shaft: '#0b1d22', tower: '#112c33',
    structure: '#4c7b82', trim: '#5f9da6', tank: '#397a83', conduit: '#62adba',
    lamp: '#82d9e5', deadLamp: '#54747a', debris: '#28454c', crack: '#75bec8',
    rock: '#0b2026', oreVein: '#3a8994', machine: '#34575e', machineDark: '#10262c',
    belt: '#111f23', ore: '#315860', fan: '#47767e', fanDark: '#0b181c', tool: '#58787e',
    rail: '#557b81', railDark: '#263e43', sleeper: '#284348', cart: '#365b61', wheel: '#111d20',
  }, true);
}

function createLabBoss(group, palette, ruined) {
  const boss = new THREE.Group();
  boss.position.set(75.0, labGroundY + 2.35, -.35);
  ring(boss, 1.62, 1.28, palette.bossShell, 0, .42, 0, ruined ? .88 : .72, 40);
  disc(boss, 1.12, palette.bossBody, 0, .42, .04, .96, 32);
  const core = disc(boss, .42, palette.bossCore, 0, .42, .12, ruined ? .98 : .72, 24);
  bossCoreMaterials.push(core.material);
  for (let index = 0; index < 6; index++) {
    const angle = index * Math.PI / 3;
    segment(boss, Math.cos(angle) * .48, .42 + Math.sin(angle) * .48, Math.cos(angle) * 1.08, .42 + Math.sin(angle) * 1.08, palette.bossTrim, ruined ? .62 : .46, .1);
  }
  const leftArm = new THREE.Group();
  leftArm.position.set(-1.25, .42, .02);
  rectangle(leftArm, 1.6, .34, palette.bossShell, -.65, 0, 0, .94);
  polygon(leftArm, [[-1.8, 0], [-1.0, .58], [-1.0, -.58]], palette.drill, -1.1, 0, .06, .92);
  boss.add(leftArm);
  const rightArm = leftArm.clone();
  rightArm.scale.x = -1;
  rightArm.position.x = 1.25;
  boss.add(rightArm);
  for (const x of [-.72, .72]) {
    rectangle(boss, .38, 1.35, palette.bossShell, x, -1.08, 0, .94);
    rectangle(boss, .82, .24, palette.bossTrim, x + (x < 0 ? -.15 : .15), -1.72, .04, .82);
  }
  if (ruined) {
    for (const [x, y, scale] of [[-.85, 1.4, .72], [.9, 1.15, .58], [0, 1.72, .65]]) {
      polygon(boss, [[0, .8 * scale], [-.34 * scale, 0], [.34 * scale, 0]], palette.crystal, x, y, .16, .9);
    }
    modernBoss = boss;
  } else {
    rectangle(boss, 3.6, .12, palette.cable, 0, 2.0, -.1, .6);
    segment(boss, -1.15, 2.0, -1.15, 1.42, palette.cable, .65, -.05);
    segment(boss, 1.15, 2.0, 1.15, 1.42, palette.cable, .65, -.05);
    pastBoss = boss;
  }
  group.add(boss);
}

function buildExpandedMine(group, palette, ruined) {
  // Gate 03 opens into the same elevator control room in both eras.
  rectangle(group, 8.0, 5.1, palette.surfaceRoom, 17.5, -1.45, -3.1, ruined ? .7 : .94);
  rectangle(group, 7.6, .24, palette.trim, 17.5, 1.15, -2.0, ruined ? .34 : .8);
  for (const x of [14.0, 16.7, 19.85, 21.0]) {
    segment(group, x, groundY, x, 1.12, palette.structure, ruined ? .32 : .7, -1.8);
  }
  rectangle(group, 1.2, 1.55, palette.panel, 15.7, groundY + .9, -.85, ruined ? .56 : .92);
  for (let index = 0; index < 4; index++) {
    disc(group, .075, ruined ? palette.deadLamp : palette.lamp, 15.45 + (index % 2) * .4, groundY + 1.18 - Math.floor(index / 2) * .4, -.65, ruined ? .35 : .92, 16);
  }

  // A continuous vertical shaft makes the descent spatial rather than a teleport.
  rectangle(group, 5.2, groundY - labGroundY + 7.0, palette.shaftRock, elevatorX, (groundY + labGroundY) / 2 + 1.5, -5.0, .9);
  segment(group, 16.65, groundY + 3.7, 16.65, labGroundY + .15, palette.structure, ruined ? .32 : .68, -2.3);
  segment(group, 19.85, groundY + 3.7, 19.85, labGroundY + .15, palette.structure, ruined ? .3 : .68, -2.3);
  for (let y = groundY + 2.9; y >= labGroundY + .8; y -= 2.1) {
    segment(group, 16.72, y, 19.78, y, palette.trim, ruined ? .26 : .58, -2.15);
    rectangle(group, .72, .18, palette.depthMark, 20.35, y, -1.8, ruined ? .24 : .68);
  }

  if (!ruined) {
    rectangle(group, .07, groundY - labGroundY + 5.8, palette.cable, 17.65, (groundY + labGroundY) / 2 + 1.55, -1.6, .72);
    rectangle(group, .07, groundY - labGroundY + 5.8, palette.cable, 18.85, (groundY + labGroundY) / 2 + 1.55, -1.6, .72);
    const car = new THREE.Group();
    car.position.set(elevatorX, groundY, -.35);
    rectangle(car, 3.0, .24, palette.trim, 0, .04, 0, .96);
    rectangle(car, .18, 3.7, palette.structure, -1.42, 1.85, 0, .9);
    rectangle(car, .18, 3.7, palette.structure, 1.42, 1.85, 0, .9);
    rectangle(car, 3.0, .24, palette.trim, 0, 3.62, 0, .92);
    rectangle(car, 2.5, .18, palette.cage, 0, 1.65, -.02, .34);
    for (const x of [-1.0, -.5, 0, .5, 1.0]) segment(car, x, .2, x, 3.45, palette.cage, .42, .04);
    rectangle(car, .42, .8, palette.panel, 1.08, 1.3, .08, .9);
    disc(car, .08, palette.lamp, 1.08, 1.48, .12, .95, 16);
    pastElevatorCar = car;
    group.add(car);
  } else {
    // At the modern shaft mouth the missing car is visible immediately: open void, torn doors and snapped cables.
    rectangle(group, 3.0, 3.65, '#061217', elevatorX, groundY + 1.85, -.72, .98);
    rectangle(group, .24, 3.9, palette.trim, 16.68, groundY + 1.95, -.5, .62);
    rectangle(group, .24, 3.9, palette.trim, 19.82, groundY + 1.95, -.5, .62);
    rectangle(group, 3.35, .26, palette.trim, elevatorX, groundY + 3.88, -.48, .68);
    const brokenDoorLeft = rectangle(group, 1.08, 3.25, palette.cage, 17.28, groundY + 1.6, -.35, .32);
    const brokenDoorRight = rectangle(group, .86, 2.4, palette.cage, 19.22, groundY + 1.15, -.35, .28);
    brokenDoorLeft.rotation.z = -.08;
    brokenDoorRight.rotation.z = .16;
    ring(group, .42, .31, palette.trim, elevatorX, groundY + 4.45, -.4, .48, 24);
    segment(group, elevatorX, groundY + 4.05, 17.75, groundY + 1.15, palette.cable, .62, -.28);
    segment(group, 17.75, groundY + 1.15, 18.05, groundY + .55, palette.cable, .46, -.28);
    segment(group, 15.35, groundY + 1.35, 16.05, groundY + .65, palette.deadLamp, .7, -.2);
    segment(group, 16.05, groundY + 1.35, 15.35, groundY + .65, palette.deadLamp, .7, -.2);
    segment(group, 17.65, groundY + 3.5, 17.65, groundY - 4.1, palette.cable, .34, -1.6);
    segment(group, 18.85, groundY + 3.5, 18.2, groundY - 5.5, palette.cable, .28, -1.6);
    const wreck = new THREE.Group();
    wreck.position.set(elevatorX, labGroundY + .35, -.4);
    wreck.rotation.z = -.12;
    rectangle(wreck, 3.0, .3, palette.trim, 0, .05, 0, .58);
    rectangle(wreck, .22, 2.2, palette.structure, -1.28, 1.0, 0, .45);
    rectangle(wreck, .22, 1.8, palette.structure, 1.15, .8, 0, .38);
    rectangle(wreck, 2.4, .18, palette.cage, 0, 1.5, .02, .28);
    group.add(wreck);
  }

  // The laboratory is about 1.3 screens wide: three readable interaction zones, not one empty hall.
  rectangle(group, 39.5, 7.5, palette.labWall, 40.0, labGroundY + 3.55, -4.2, ruined ? .75 : .96);
  rectangle(group, 31.0, 8.2, palette.bossRoom, 75.2, labGroundY + 3.9, -4.35, ruined ? .78 : .94);
  rectangle(group, 70.5, .28, palette.trim, 55.0, labGroundY + 7.35, -2.9, ruined ? .34 : .76);
  rectangle(group, 70.5, .34, palette.floor, 55.0, labGroundY - .16, -2.7, ruined ? .7 : .95);
  for (const x of [21.0, 29.0, 37.0, 45.0, 53.0, 59.2, 62.0, 70.0, 80.0, 90.0]) {
    const column = rectangle(group, .2, 7.25, palette.structure, x, labGroundY + 3.55, -2.6, ruined ? .36 : .72);
    if (ruined && (x === 37.0 || x === 70.0)) column.rotation.z = x < 50 ? -.06 : .045;
  }

  // Laboratory entrance: powered and permission-locked in 2047, collapsed open in 2147.
  rectangle(group, .28, 7.1, palette.trim, labEntranceX - .9, labGroundY + 3.5, -1.15, .92);
  rectangle(group, .28, 7.1, palette.trim, labEntranceX + .9, labGroundY + 3.5, -1.15, .92);
  rectangle(group, 2.1, .28, palette.trim, labEntranceX, labGroundY + 7.0, -1.12, .92);
  if (!ruined) {
    rectangle(group, 1.5, 6.35, palette.bulkhead, labEntranceX, labGroundY + 3.2, -.96, .98);
    for (let y = labGroundY + .55; y <= labGroundY + 5.9; y += .72) {
      segment(group, labEntranceX - .62, y, labEntranceX + .62, y, palette.archGlow, .42, -.72);
    }
    for (let index = 0; index < 4; index++) {
      disc(group, .075, palette.lamp, labEntranceX - 1.2, labGroundY + 5.25 - index * .5, -.7, .88, 14);
    }
  } else {
    rectangle(group, 1.55, 6.1, '#07161b', labEntranceX, labGroundY + 3.1, -1.02, .98);
    const fallenDoor = rectangle(group, 1.5, 4.2, palette.bulkhead, labEntranceX + .72, labGroundY + .95, -.78, .58);
    fallenDoor.rotation.z = -1.17;
    segment(group, labEntranceX - .55, labGroundY + 5.8, labEntranceX + .35, labGroundY + 4.9, palette.cable, .46, -.65);
    segment(group, labEntranceX + .35, labGroundY + 4.9, labEntranceX + .05, labGroundY + 3.85, palette.cable, .34, -.65);
  }

  // Sample storage area.
  rectangle(group, 6.3, .28, palette.trim, 26.0, labGroundY + 1.05, -1.7, ruined ? .4 : .82);
  for (const x of [23.8, 25.4, 27.0, 28.6]) {
    rectangle(group, 1.0, 2.65, palette.glass, x, labGroundY + 2.55, -1.6, ruined ? .28 : .48);
    ring(group, .46, .38, palette.trim, x, labGroundY + 3.0, -1.35, ruined ? .34 : .76, 24);
    polygon(group, [[0, .55], [-.3, -.35], [.3, -.35]], palette.crystal, x, labGroundY + 2.65, -1.25, ruined ? .38 : .8);
  }

  // Time observation arch: the repeated ring remains recognizable after a century.
  ring(group, 2.05, 1.72, palette.arch, 36.0, labGroundY + 3.05, -1.5, ruined ? .46 : .9, 48);
  ring(group, 1.48, 1.37, palette.archGlow, 36.0, labGroundY + 3.05, -1.35, ruined ? .2 : .72, 48);
  for (let index = 0; index < 8; index++) {
    const angle = index * Math.PI / 4;
    disc(group, .09, palette.archGlow, 36 + Math.cos(angle) * 1.88, labGroundY + 3.05 + Math.sin(angle) * 1.88, -1.2, ruined ? .26 : .9, 16);
  }

  // Chronite cultivation device and its manual emergency cutoff.
  rectangle(group, 5.6, 5.4, palette.reactorFrame, 45.4, labGroundY + 3.15, -1.7, ruined ? .46 : .9);
  rectangle(group, 2.25, 4.35, palette.glass, 44.7, labGroundY + 3.1, -1.45, ruined ? .24 : .46);
  ring(group, 1.0, .87, palette.trim, 44.7, labGroundY + 3.1, -1.18, ruined ? .38 : .82, 36);
  if (!ruined) {
    const reactorCore = new THREE.Group();
    reactorCore.position.set(44.7, labGroundY + 3.1, -.95);
    polygon(reactorCore, [[0, 1.05], [-.62, -.2], [-.28, -.95], [.45, -.62], [.72, .15]], palette.reactorCore, 0, 0, 0, .96);
    ring(reactorCore, 1.35, 1.28, palette.archGlow, 0, 0, -.05, .58, 40);
    pastReactorCore = reactorCore;
    group.add(reactorCore);
    rectangle(group, 1.3, 1.4, palette.panel, reactorControlX, labGroundY + 1.2, -.9, .94);
    const lever = rectangle(group, .18, .72, palette.lever, reactorControlX, labGroundY + 1.55, -.65, .96);
    lever.rotation.z = -.45;
    pastShutdownLever = lever;
    for (let index = 0; index < 3; index++) disc(group, .075, palette.lamp, 46.8 + index * .38, labGroundY + .82, -.62, .9, 14);
  } else {
    rectangle(group, 1.3, 1.4, palette.panel, reactorControlX, labGroundY + 1.2, -.9, .52);
    const inert = new THREE.Group();
    inert.position.set(44.7, labGroundY + 2.75, -.9);
    polygon(inert, [[0, .48], [-.35, -.22], [.28, -.38], [.46, .12]], palette.inertCore, 0, 0, 0, .78);
    modernInertCore = inert;
    group.add(inert);

    const growth = new THREE.Group();
    growth.position.z = -.65;
    const crystals = [
      [44.7, labGroundY + 3.25, 1.7, 0], [47.0, labGroundY + 1.1, 1.35, -.35],
      [50.0, labGroundY + .85, 1.6, .28], [53.2, labGroundY + 1.35, 1.75, -.22],
      [56.7, labGroundY + 2.0, 2.15, .08], [57.4, labGroundY + 4.4, 2.4, -.1],
    ];
    for (const [x, y, scale, rotation] of crystals) {
      const crystal = polygon(growth, [[0, 1.0 * scale], [-.38 * scale, -.65 * scale], [.4 * scale, -.65 * scale]], palette.growth, x, y, 0, .88);
      crystal.rotation.z = rotation;
    }
    segment(growth, 44.7, labGroundY + 3.0, 57.2, labGroundY + 1.6, palette.growthLine, .7, .05);
    segment(growth, 48.0, labGroundY + 1.2, 57.4, labGroundY + 5.8, palette.growthLine, .58, .05);
    modernOvergrowth = growth;
    group.add(growth);
  }

  // Boss airlock and one-screen arena.
  rectangle(group, .28, 7.1, palette.trim, 59.2, labGroundY + 3.5, -1.2, .92);
  rectangle(group, .28, 7.1, palette.trim, 61.2, labGroundY + 3.5, -1.2, .92);
  rectangle(group, 2.3, .28, palette.trim, 60.2, labGroundY + 7.0, -1.15, .92);
  if (!ruined) rectangle(group, 1.55, 6.35, palette.bulkhead, 60.2, labGroundY + 3.25, -1.0, .94);
  if (ruined) {
    const battleBarrier = new THREE.Group();
    for (let y = labGroundY + .35; y <= labGroundY + 6.6; y += .48) {
      disc(battleBarrier, .075, palette.archGlow, 62.45, y, -.62, .82, 14);
    }
    rectangle(battleBarrier, .09, 6.55, palette.archGlow, 62.45, labGroundY + 3.45, -.68, .34);
    bossBattleBarrier = battleBarrier;
    group.add(battleBarrier);
  }
  rectangle(group, 5.2, .26, palette.platform, 67.0, labGroundY + 2.0, -1.1, ruined ? .62 : .78);
  rectangle(group, 5.2, .26, palette.platform, 83.2, labGroundY + 2.0, -1.1, ruined ? .62 : .78);
  for (const x of [64.8, 69.2, 81.0, 85.4]) rectangle(group, .16, 2.0, palette.structure, x, labGroundY + 1.0, -1.2, ruined ? .38 : .65);
  createLabBoss(group, palette, ruined);

  const lampXs = [23.0, 28.5, 34.0, 40.0, 46.0, 52.0, 56.0, 65.0, 72.0, 79.0, 86.0];
  for (const x of lampXs) {
    segment(group, x, labGroundY + 7.2, x, labGroundY + 6.65, palette.structure, ruined ? .28 : .58, -1.4);
    const lamp = disc(group, .16, ruined ? palette.deadLamp : palette.lamp, x, labGroundY + 6.5, -1.15, ruined ? .3 : .95, 18);
    if (!ruined) pastLampMaterials.push(lamp.material);
  }
}

function buildPastExpansion() {
  buildExpandedMine(pastLayer, {
    surfaceRoom: '#301719', trim: '#d17645', structure: '#9e5738', panel: '#51251f', lamp: '#ffb15d',
    deadLamp: '#774534', shaftRock: '#1e1013', depthMark: '#d9854e', cable: '#e58449', cage: '#b96a43',
    labWall: '#35181b', bossRoom: '#291417', floor: '#6f3728', glass: '#a9573a', crystal: '#ff9d52',
    arch: '#d27645', archGlow: '#ffb45f', reactorFrame: '#572620', reactorCore: '#ff8c45', lever: '#ffcc72',
    inertCore: '#a86a45', bulkhead: '#4b211e', platform: '#995333', bossShell: '#9d5535', bossBody: '#4b211e',
    bossCore: '#ffb65f', bossTrim: '#d67a45', drill: '#d98a54', growth: '#ff8f48', growthLine: '#ffb05b',
  }, false);
}

function buildPresentExpansion() {
  buildExpandedMine(presentLayer, {
    surfaceRoom: '#10252b', trim: '#5f9da6', structure: '#456e75', panel: '#142e34', lamp: '#82d9e5',
    deadLamp: '#526d72', shaftRock: '#09191e', depthMark: '#4e7a81', cable: '#4d7379', cage: '#42666c',
    labWall: '#10272d', bossRoom: '#0b1d22', floor: '#203a40', glass: '#30545b', crystal: '#4c8d96',
    arch: '#4f858d', archGlow: '#75d6e2', reactorFrame: '#17343b', reactorCore: '#5ecbd8', lever: '#79c9d2',
    inertCore: '#54767b', bulkhead: '#17343a', platform: '#365d64', bossShell: '#4e858d', bossBody: '#142b31',
    bossCore: '#82edf6', bossTrim: '#70bdc6', drill: '#6ba9b1', growth: '#4fb4c1', growthLine: '#70d9e4',
  }, true);
}

buildPastScene();
buildPresentScene();
buildPastExpansion();
buildPresentExpansion();

const commonLayer = new THREE.Group();
scene.add(commonLayer);

// 地面以下不是空画布，而是可见的岩层剖面与更深一层的排水巷道。
const bedrockMaterial = material('#122b31', 1);
const bedrock = new THREE.Mesh(new THREE.PlaneGeometry(46, 8.5), bedrockMaterial);
bedrock.position.set(2.5, groundY - 4.5, -5.8);
commonLayer.add(bedrock);

const deepBedrockMaterial = material('#0d242a', 1);
const deepBedrock = new THREE.Mesh(new THREE.PlaneGeometry(82, 7.5), deepBedrockMaterial);
deepBedrock.position.set(55, labGroundY - 3.9, -5.8);
commonLayer.add(deepBedrock);

const strataMaterial = lineMaterial('#3e6870', .34);
for (let row = 0; row < 6; row++) {
  const y = groundY - .75 - row * .72;
  const strata = new THREE.Line(
    new THREE.BufferGeometry().setFromPoints([
      new THREE.Vector3(-18, y + .12, -5.4),
      new THREE.Vector3(-11, y - .08, -5.4),
      new THREE.Vector3(-4, y + .14, -5.4),
      new THREE.Vector3(3, y - .12, -5.4),
      new THREE.Vector3(10, y + .08, -5.4),
      new THREE.Vector3(18, y - .06, -5.4),
    ]),
    strataMaterial,
  );
  commonLayer.add(strata);
}

const undergroundOreMaterial = material('#3b7d86', .55);
for (const [x, y, radius] of [[-13.8, -5.85, .26], [-12.9, -6.2, .17], [-7.1, -5.62, .21], [-1.6, -6.0, .3], [5.6, -5.7, .2], [13.3, -6.15, .28]]) {
  const ore = new THREE.Mesh(new THREE.CircleGeometry(radius, 8), undergroundOreMaterial);
  ore.position.set(x, y, -5.1);
  ore.rotation.z = x * .13;
  commonLayer.add(ore);
}

const lowerTunnelMaterial = material('#081419', .95);
const lowerTunnel = polygon(commonLayer, [
  [-5.4, -.8], [-4.8, .55], [-3.8, 1.05], [3.7, 1.05], [4.8, .55], [5.4, -.8],
], '#081419', 5.2, groundY - 3.25, -4.7, .95);
lowerTunnel.material = lowerTunnelMaterial;
for (const x of [1.3, 4.0, 6.7, 9.4]) {
  segment(commonLayer, x, groundY - 4.1, x, groundY - 2.35, '#38535a', .42, -4.45);
  segment(commonLayer, x, groundY - 2.35, x + .55, groundY - 1.85, '#38535a', .42, -4.45);
}
segment(commonLayer, 1.3, groundY - 2.35, 9.95, groundY - 2.35, '#38535a', .42, -4.44);

const groundMaterial = material('#274048', 1);
const ground = new THREE.Mesh(new THREE.PlaneGeometry(45, .56), groundMaterial);
ground.position.set(3.0, groundY - .28, .2);
commonLayer.add(ground);

for (let x = -15.5; x <= 22.0; x += 1.05) {
  segment(commonLayer, x, groundY + .01, x + .72, groundY + .01, '#789099', .18, .3);
}

const labGroundMaterial = material('#203a40', 1);
const labGround = new THREE.Mesh(new THREE.PlaneGeometry(78, .6), labGroundMaterial);
labGround.position.set(55, labGroundY - .3, .18);
commonLayer.add(labGround);
for (let x = 16.2; x <= 93.0; x += 1.1) {
  segment(commonLayer, x, labGroundY + .015, x + .76, labGroundY + .015, '#789099', .2, .3);
}

function createHandwheel(color, opacity = 1) {
  const group = new THREE.Group();
  const ringPoints = Array.from({ length: 49 }, (_, index) => {
    const angle = index / 48 * Math.PI * 2;
    return new THREE.Vector3(Math.cos(angle) * .58, Math.sin(angle) * .58, .08);
  });
  group.add(new THREE.LineLoop(new THREE.BufferGeometry().setFromPoints(ringPoints), lineMaterial(color, opacity)));
  disc(group, .13, '#101b1e', 0, 0, .1, .96);
  for (let index = 0; index < 8; index++) {
    const angle = index / 8 * Math.PI * 2;
    segment(group, Math.cos(angle) * .12, Math.sin(angle) * .12, Math.cos(angle) * .54, Math.sin(angle) * .54, color, opacity, .09);
  }
  return group;
}

function addStationNumber(group, color, opacity, yOffset = 0) {
  segment(group, -.72, 3.2 + yOffset, -.72, 4.05 + yOffset, color, opacity, .15);
  segment(group, -.72, 4.05 + yOffset, -.28, 4.05 + yOffset, color, opacity, .15);
  segment(group, -.28, 4.05 + yOffset, -.28, 3.2 + yOffset, color, opacity, .15);
  segment(group, -.28, 3.2 + yOffset, -.72, 3.2 + yOffset, color, opacity, .15);
  segment(group, .08, 4.05 + yOffset, .55, 4.05 + yOffset, color, opacity, .15);
  segment(group, .55, 4.05 + yOffset, .55, 3.2 + yOffset, color, opacity, .15);
  segment(group, .08, 3.64 + yOffset, .55, 3.64 + yOffset, color, opacity, .15);
  segment(group, .08, 3.2 + yOffset, .55, 3.2 + yOffset, color, opacity, .15);
}

const gateX = 8.35;
const winchSocketX = 5.25;
const handwheelPickupX = winchSocketX;

function createGate(frameColor, doorColor, edgeColor, powered) {
  const group = new THREE.Group();
  group.position.set(gateX, groundY, .88);
  rectangle(group, .28, 6.4, frameColor, -1.0, 3.2, 0, .96);
  rectangle(group, .28, 6.4, frameColor, 1.0, 3.2, 0, .96);
  rectangle(group, 2.3, .34, frameColor, 0, 6.18, .01, .96);

  const door = new THREE.Group();
  rectangle(door, 1.55, 5.15, doorColor, 0, 0, .03, .98);
  for (let y = -2.1; y <= 2.1; y += .7) {
    segment(door, -.67, y, .67, y, edgeColor, .62, .08);
  }
  segment(door, -.64, -2.35, .64, 2.35, edgeColor, .32, .07);
  segment(door, .64, -2.35, -.64, 2.35, edgeColor, .32, .07);
  addStationNumber(door, edgeColor, .82, -3.63);
  door.position.y = 2.58;
  group.add(door);

  const indicator = disc(group, .14, powered ? '#ffac59' : '#648990', -1.0, 5.55, .08, .9);
  segment(group, -1.35, 4.95, -1.35, .75, edgeColor, .3, .06);
  if (powered) {
    pastDoorPowerMaterials.push(indicator.material);
    rectangle(group, .08, 5.1, '#ff9347', -.84, 3.05, .11, .38);
    rectangle(group, .08, 5.1, '#ff9347', .84, 3.05, .11, .38);
    for (let index = 0; index < 5; index++) {
      const lockLight = disc(group, .075, '#ffbd68', -1.32, 4.55 - index * .52, .12, .74);
      pastDoorPowerMaterials.push(lockLight.material);
    }
    segment(group, -1.55, 1.1, -1.12, 1.53, '#ffb15d', .55, .13);
    segment(group, -1.55, 1.53, -1.12, 1.96, '#ffb15d', .55, .13);
    segment(group, -1.55, 1.96, -1.12, 2.39, '#ffb15d', .55, .13);
  }
  scene.add(group);
  return { group, door, indicator };
}

const pastGate = createGate('#77462e', '#4d251f', '#ffad5b', true);
const modernGate = createGate('#29464d', '#18353c', '#82d6df', false);

function createSocket(color, glow) {
  const group = new THREE.Group();
  group.position.set(winchSocketX, groundY, .82);
  rectangle(group, 1.85, .2, color, 0, .1, 0, .9);
  rectangle(group, .16, 1.8, color, -.82, .92, 0, .78);
  rectangle(group, .16, 1.8, color, .82, .92, 0, .78);
  segment(group, -.62, 1.62, .62, 1.62, glow, .68, .08);
  for (let x = -.45; x <= .45; x += .3) disc(group, .07, glow, x, .52, .08, .72);
  scene.add(group);
  return group;
}

const pastSocket = createSocket('#6d422d', '#ffb15d');
const modernSocket = createSocket('#29464d', '#82d6df');

function createWinch(baseColor, edgeColor) {
  const group = new THREE.Group();
  group.position.set(winchSocketX, groundY, 1.02);
  rectangle(group, 1.45, 1.18, baseColor, 0, .72, 0, .96);
  disc(group, .43, '#101b1e', 0, .78, .06, .95);
  const drum = disc(group, .31, baseColor, 0, .78, .08, 1);
  segment(group, .48, 1.23, gateX - winchSocketX - .35, 4.82, edgeColor, .45, .09);
  rectangle(group, .55, .22, edgeColor, -.42, .23, .08, .72);
  scene.add(group);
  return { group, drum };
}

const pastWinch = createWinch('#70402a', '#ffb15d');
const modernWinch = createWinch('#29464d', '#82d6df');
const pastHandwheelPickup = createHandwheel('#ffb15d');
pastHandwheelPickup.position.set(winchSocketX, groundY + .78, 1.24);
scene.add(pastHandwheelPickup);
const modernBrokenWheel = createHandwheel('#567076', .52);
modernBrokenWheel.position.set(winchSocketX, groundY + .78, 1.22);
modernBrokenWheel.rotation.z = .16;
segment(modernBrokenWheel, -.42, .3, .05, -.08, '#9ac0c5', .35, .12);
segment(modernBrokenWheel, .05, -.08, .5, -.45, '#9ac0c5', .28, .12);
scene.add(modernBrokenWheel);
const modernInstalledWheel = createHandwheel('#82d6df');
modernInstalledWheel.position.set(winchSocketX, groundY + .78, 1.24);
scene.add(modernInstalledWheel);

const exitGroup = new THREE.Group();
exitGroup.position.set(12.9, -1.3, -.2);
scene.add(exitGroup);
const exitGlow = disc(exitGroup, 2.35, '#7ce5f2', 0, 0, 0, .06);
const exitCore = rectangle(exitGroup, .12, 4.4, '#a5f5ff', 0, 0, .1, .6);
exitGroup.visible = false;

function createPlayer() {
  const group = new THREE.Group();
  const bodyMat = material('#dffaff', .96);
  const trimMat = material('#75cbd6', .86);
  const shadowMat = material('#000000', .24);

  const shadow = new THREE.Mesh(new THREE.CircleGeometry(.58, 24), shadowMat);
  shadow.scale.y = .24;
  shadow.position.set(0, -.83, -.1);
  group.add(shadow);

  const bodyRig = new THREE.Group();
  group.add(bodyRig);
  const body = new THREE.Mesh(new THREE.PlaneGeometry(.48, .7), bodyMat);
  body.position.y = -.08;
  bodyRig.add(body);
  const chestTrim = rectangle(bodyRig, .56, .13, '#75cbd6', 0, .19, .05, .9);
  chestTrim.material = trimMat;

  const headRig = new THREE.Group();
  headRig.position.y = .55;
  const head = new THREE.Mesh(new THREE.CircleGeometry(.31, 12), bodyMat);
  headRig.add(head);
  rectangle(headRig, .22, .16, '#0e272d', .11, .02, .08, .95);
  group.add(headRig);

  function makeArm(x, y) {
    const limb = new THREE.Group();
    limb.position.set(x, y, .06);
    const limbBody = rectangle(limb, .16, .39, '#e9fdff', 0, -.195, .08, .96);
    limbBody.material = bodyMat;
    disc(limb, .086, '#e9fdff', 0, 0, .09, .96, 12).material = bodyMat;
    disc(limb, .083, '#e9fdff', 0, -.39, .1, .96, 12).material = bodyMat;
    group.add(limb);
    return limb;
  }

  function makeLeg(x) {
    const hip = new THREE.Group();
    hip.position.set(x, -.4, .06);
    const upper = rectangle(hip, .19, .25, '#e9fdff', 0, -.125, .08, .98);
    upper.material = bodyMat;
    const knee = new THREE.Group();
    knee.position.set(0, -.245, .02);
    disc(knee, .098, '#e9fdff', 0, 0, .09, .98, 12).material = bodyMat;
    const lower = rectangle(knee, .18, .25, '#e9fdff', 0, -.125, .08, .98);
    lower.material = bodyMat;
    const boot = rectangle(knee, .29, .14, '#75cbd6', .07, -.27, .11, .98);
    boot.material = trimMat;
    hip.add(knee);
    group.add(hip);
    return { hip, knee };
  }

  const leftLegRig = makeLeg(-.13);
  const rightLegRig = makeLeg(.13);
  const leftLeg = leftLegRig.hip;
  const rightLeg = rightLegRig.hip;
  const leftArm = makeArm(-.3, .12);
  const rightArm = makeArm(.3, .12);

  group.userData.bodyMaterials = [bodyMat];
  group.userData.accentMaterials = [trimMat];
  group.userData.rig = {
    bodyRig, headRig, shadow, leftLeg, rightLeg, leftKnee: leftLegRig.knee, rightKnee: rightLegRig.knee, leftArm, rightArm,
    bodyBaseY: bodyRig.position.y,
    headBaseY: headRig.position.y,
    leftArmBaseY: leftArm.position.y,
    rightArmBaseY: rightArm.position.y,
  };
  return group;
}

const playerMesh = createPlayer();
scene.add(playerMesh);

function createSword() {
  const group = new THREE.Group();
  polygon(group, [[0, -.075], [1.18, -.075], [1.48, 0], [1.18, .075], [0, .075]], '#d9f8fb', .05, 0, .2, .98);
  rectangle(group, .12, .46, '#d39a59', -.03, 0, .22, .98).rotation.z = Math.PI / 2;
  rectangle(group, .38, .09, '#96613c', -.2, 0, .23, .98);
  return group;
}

function createSpear() {
  const group = new THREE.Group();
  rectangle(group, 2.35, .07, '#a76e43', 1.0, 0, .2, .98);
  polygon(group, [[0, -.18], [.52, 0], [0, .18]], '#d8f8fb', 2.18, 0, .22, .98);
  rectangle(group, .24, .11, '#d39a59', -.18, 0, .22, .95);
  return group;
}

const weaponRig = new THREE.Group();
weaponRig.position.set(.22, -.02, .45);
const swordMesh = createSword();
const spearMesh = createSpear();
weaponRig.add(swordMesh, spearMesh);
playerMesh.add(weaponRig);
const slashMesh = new THREE.Mesh(
  new THREE.RingGeometry(.68, 1.48, 28, 1, -.9, 1.8),
  material('#b8f5fa', .22),
);
slashMesh.position.set(.28, .02, .3);
slashMesh.visible = false;
playerMesh.add(slashMesh);

const bossProjectileLayer = new THREE.Group();
scene.add(bossProjectileLayer);

const particleCount = 360;
const particlePositions = new Float32Array(particleCount * 3);
for (let index = 0; index < particleCount; index++) {
  particlePositions[index * 3] = THREE.MathUtils.randFloat(-17, 94);
  particlePositions[index * 3 + 1] = THREE.MathUtils.randFloat(-27, 7);
  particlePositions[index * 3 + 2] = THREE.MathUtils.randFloat(-5, 4);
}
const particleGeometry = new THREE.BufferGeometry();
particleGeometry.setAttribute('position', new THREE.BufferAttribute(particlePositions, 3));
const particleMaterial = new THREE.PointsMaterial({ color: '#82d9e5', size: .055, transparent: true, opacity: .32 });
const particles = new THREE.Points(particleGeometry, particleMaterial);
scene.add(particles);

function showToast(message) {
  toast.textContent = message;
  toast.classList.add('show');
  clearTimeout(showToast.timer);
  showToast.timer = setTimeout(() => toast.classList.remove('show'), 1900);
}

function flashTime() {
  timeFlash.classList.remove('active');
  void timeFlash.offsetWidth;
  timeFlash.classList.add('active');
}

function updateInventoryHud() {
  const hasWheel = state.inventory.handwheel;
  handwheelSlot.classList.toggle('empty', !hasWheel);
  hotbarWheel.classList.toggle('empty', !hasWheel);
  hotbarSword.classList.toggle('selected', state.selectedWeapon === 'sword');
  hotbarSpear.classList.toggle('selected', state.selectedWeapon === 'spear');
  if (state.selectedWeapon === 'spear') {
    inventoryItemName.textContent = '矿用长矛';
    inventoryItemStatus.textContent = '快捷栏 2 · 较远距离直刺 · 伤害 24 · 不会消耗';
  } else {
    inventoryItemName.textContent = '矿用长剑';
    inventoryItemStatus.textContent = '快捷栏 1 · 近距离横斩 · 伤害 38 · 不会消耗';
  }
}

function selectWeapon(weapon) {
  state.selectedWeapon = weapon;
  updateHud();
}

function setInventoryOpen(open) {
  state.inventoryOpen = open;
  inventoryPanel.classList.toggle('open', open);
  inventoryPanel.setAttribute('aria-hidden', String(!open));
  keys.clear();
  updateInventoryHud();
}

function toggleInventory() {
  setInventoryOpen(!state.inventoryOpen);
}

function toggleEra() {
  if (state.bossAwake && !state.boss.defeated) {
    showToast('Boss的时间扰动场正在封锁时代切换；击败构装体后才能离开');
    return;
  }
  if (state.elevatorRiding) {
    showToast('升降机强电磁场正在干扰时间锚；到站后才能切换时代');
    return;
  }
  const now = clock.elapsedTime;
  if (now - state.lastToggle < .55) return;
  setInventoryOpen(false);
  state.lastToggle = now;
  state.eraTarget = state.eraTarget > .5 ? 0 : 1;
  state.pulse = 1;
  document.body.classList.toggle('past', state.eraTarget < .5);
  eraLabel.textContent = state.eraTarget < .5 ? '过去 · 2047' : '现代 · 2147';
  flashTime();

  const inLab = player.y < -12;
  if (state.eraTarget < .5 && inLab) {
    showToast('2047年：地下实验室仍在运行，时间矿物培养装置尚未失控');
  } else if (state.eraTarget < .5) {
    showToast('固定时间点：2047年，03号矿场仍在正常运行');
  } else if (inLab && state.history.experimentShutdown) {
    state.history.futureCleared = true;
    showToast('因果改写生效：2147年的异常结晶没有形成，Boss通道已经露出');
  } else if (inLab) {
    showToast('2147年：培养装置持续运行百年，异常结晶已经封死前方通道');
  } else if (state.history.wheelInstalled) {
    showToast('固定时间点：2147年，跨时带来的手轮仍安装在03号闸门上');
  } else if (state.inventory.handwheel) {
    state.history.wheelCrossed = true;
    showToast('固定时间点：2147年，时间锚背包把2047年的手轮带了过来');
  } else {
    showToast('固定时间点：2147年，闸门仍因缺少手动开门盘而锁死');
  }
  updateHud();
}

function resetHistory() {
  state.eraTarget = 1;
  state.era = 1;
  state.pulse = 1;
  state.gateLift = 0;
  state.exitReached = false;
  state.elevatorY = groundY;
  state.elevatorTargetY = labGroundY;
  state.elevatorRiding = false;
  state.elevatorAtBottom = false;
  state.bossAwake = false;
  state.selectedWeapon = 'sword';
  state.attackTimer = 0;
  state.attackDuration = 0;
  state.attackCooldown = 0;
  state.attackHit = false;
  state.boss.health = state.boss.maxHealth;
  state.boss.x = 75;
  state.boss.vx = 0;
  state.boss.decisionTimer = 1.4;
  state.boss.chargeTimer = 0;
  state.boss.projectileTimer = 1.2;
  state.boss.hitFlash = 0;
  state.boss.defeated = false;
  state.cameraX = 0;
  state.cameraY = 0;
  state.inventoryOpen = false;
  state.inventory.handwheel = false;
  state.history.wheelCollected = false;
  state.history.wheelCrossed = false;
  state.history.wheelInstalled = false;
  state.history.gateOpened = false;
  state.history.elevatorUsed = false;
  state.history.experimentShutdown = false;
  state.history.futureCleared = false;
  inventoryPanel.classList.remove('open');
  inventoryPanel.setAttribute('aria-hidden', 'true');
  player.x = -10.4;
  player.y = groundY + player.halfH;
  player.vx = 0;
  player.vy = 0;
  player.walkPhase = 0;
  player.walkBlend = 0;
  player.health = player.maxHealth;
  player.hurtCooldown = 0;
  for (const projectile of bossProjectiles) bossProjectileLayer.remove(projectile.mesh);
  bossProjectiles.length = 0;
  document.body.classList.remove('past');
  eraLabel.textContent = '现代 · 2147';
  flashTime();
  showToast('时间线已重置');
  updateHud();
}

function overlaps(aCenter, aHalf, bCenter, bHalf) {
  return Math.abs(aCenter - bCenter) < aHalf + bHalf;
}

function isLowerLevel() {
  return player.y < (groundY + labGroundY) / 2;
}

function updateElevator(dt) {
  if (!state.elevatorRiding) return;
  const direction = Math.sign(state.elevatorTargetY - state.elevatorY);
  const remaining = Math.abs(state.elevatorTargetY - state.elevatorY);
  state.elevatorY += direction * Math.min(remaining, dt * 4.35);
  player.x = elevatorX;
  player.y = state.elevatorY + player.halfH;
  player.vx = 0;
  player.vy = 0;
  player.grounded = true;
  if (remaining <= dt * 4.35 + .015) {
    state.elevatorY = state.elevatorTargetY;
    state.elevatorRiding = false;
    state.elevatorAtBottom = state.elevatorTargetY === labGroundY;
    if (state.elevatorAtBottom) {
      state.history.elevatorUsed = true;
      showToast('抵达地下层：2047年的实验室安全门锁死，按 Q 去2147年穿过坍塌入口');
    } else {
      showToast('升降机已返回2047年地面层；切到2147年即可穿过已经打开的03号门离开');
    }
    updateHud();
  }
}

function updateHorizontal(dt) {
  if (state.elevatorRiding) return;
  const direction = state.inventoryOpen
    ? 0
    : (keys.has('KeyD') ? 1 : 0) - (keys.has('KeyA') ? 1 : 0);
  player.vx = THREE.MathUtils.damp(player.vx, direction * player.speed, direction ? 15 : 22, dt);
  if (direction) player.facing = direction;
  const lowerLevel = isLowerLevel();
  const minX = lowerLevel ? 16.9 : -14.7;
  const maxX = lowerLevel ? 91.0 : 20.25;
  let nextX = THREE.MathUtils.clamp(player.x + player.vx * dt, minX, maxX);
  const playerTop = player.y + player.halfH;

  if (!lowerLevel) {
    const gateBottom = groundY + (state.eraTarget < .5 ? 0 : state.gateLift * 5.2);
    const gateVerticalOverlap = playerTop > gateBottom + .05;
    if (gateVerticalOverlap && overlaps(nextX, player.halfW, gateX, .72)) {
      nextX = player.x < gateX
        ? gateX - .72 - player.halfW
        : gateX + .72 + player.halfW;
      player.vx = 0;
    }
  } else {
    const blockedByPastLabDoor = state.eraTarget < .5 && overlaps(nextX, player.halfW, labEntranceX, .72);
    const blockedByPastBulkhead = state.eraTarget < .5 && overlaps(nextX, player.halfW, 60.2, .72);
    const blockedByFutureGrowth = state.eraTarget > .5
      && !state.history.experimentShutdown
      && overlaps(nextX, player.halfW, futureBarrierX, .9);
    const blockedByBattleGate = state.eraTarget > .5
      && state.bossAwake
      && !state.boss.defeated
      && overlaps(nextX, player.halfW, 62.45, .34);
    if (blockedByPastLabDoor || blockedByPastBulkhead || blockedByFutureGrowth || blockedByBattleGate) {
      const obstacleX = blockedByPastLabDoor
        ? labEntranceX
        : (blockedByPastBulkhead ? 60.2 : (blockedByFutureGrowth ? futureBarrierX : 62.45));
      nextX = player.x < obstacleX
        ? obstacleX - .9 - player.halfW
        : obstacleX + .9 + player.halfW;
      player.vx = 0;
    }
  }
  player.x = nextX;
}

function updateVertical(dt) {
  if (state.elevatorRiding) return;
  player.vy -= 24 * dt;
  let nextY = player.y + player.vy * dt;
  const landingY = isLowerLevel() ? labGroundY : groundY;

  const nextBottom = nextY - player.halfH;
  if (player.vy <= 0 && nextBottom <= landingY) {
    nextY = landingY + player.halfH;
    player.vy = 0;
    player.grounded = true;
  } else {
    player.grounded = false;
  }
  player.y = nextY;
}

function tryJump() {
  if (state.inventoryOpen || state.elevatorRiding || !player.grounded) return;
  player.vy = player.jumpSpeed;
  player.grounded = false;
}

function handleInteraction() {
  if (state.inventoryOpen) return;
  const lowerLevel = isLowerLevel();
  const nearElevator = Math.abs(player.x - elevatorX) < 1.85;
  const nearLabEntrance = lowerLevel && Math.abs(player.x - labEntranceX) < 1.65;
  const nearReactorControl = lowerLevel && Math.abs(player.x - reactorControlX) < 1.65;
  const playerNearPickup = Math.abs(player.x - handwheelPickupX) < 1.75;
  const playerNearSocket = Math.abs(player.x - winchSocketX) < 2.0;

  if (nearElevator) {
    if (state.eraTarget > .5) {
      showToast(lowerLevel
        ? '2147年井底只剩坠毁轿厢；按 Q 回2047年，使用完好的轿厢返回地面'
        : '2147年井口是空的：轿厢坠毁、钢缆断裂；回2047年乘完整电梯');
    } else if (lowerLevel && state.elevatorAtBottom) {
      state.elevatorRiding = true;
      state.elevatorTargetY = groundY;
      player.x = elevatorX;
      player.vx = 0;
      keys.clear();
      showToast('2047年升降机返程启动：正在上升到地面层');
      updateHud();
    } else if (!lowerLevel && state.elevatorAtBottom) {
      showToast('2047年的轿厢当前停在实验室层');
    } else {
      state.elevatorRiding = true;
      state.elevatorY = groundY;
      state.elevatorTargetY = labGroundY;
      player.x = elevatorX;
      player.vx = 0;
      keys.clear();
      showToast('2047年升降机启动：下降期间强电磁场会暂时干扰时间锚');
      updateHud();
    }
    return;
  }

  if (nearLabEntrance) {
    showToast(state.eraTarget < .5
      ? '2047年实验室安全门受权限锁定；按 Q 回2147年，从已经坍塌的同一扇门进入'
      : '2147年门扇和锁具已经坍塌，入口可以直接穿过');
    return;
  }

  if (nearReactorControl) {
    if (state.eraTarget > .5) {
      showToast(state.history.experimentShutdown
        ? '2147年：装置早已停机，培养舱中只剩没有增生的惰性矿物'
        : '2147年：控制台已经被结晶吞没；必须在2047年阻止培养实验继续运行');
    } else if (!state.history.experimentShutdown) {
      state.history.experimentShutdown = true;
      state.pulse = 1;
      showToast('2047年操作：拉下紧急断路杆，时间矿物培养装置永久停止供能');
      updateHud();
    } else {
      showToast('2047年：紧急断路杆已经锁死，培养装置不会重新启动');
    }
    return;
  }

  if (lowerLevel) {
    if (state.eraTarget > .5 && !state.history.experimentShutdown && Math.abs(player.x - futureBarrierX) < 2.2) {
      showToast('这些结晶从培养装置一直增生到2147年；回到2047年切断它的能源');
    } else if (state.eraTarget < .5 && Math.abs(player.x - 60.2) < 2.0) {
      showToast('2047年Boss实验区受安全权限封锁；改变实验历史后从2147年进入');
    }
    return;
  }

  if (state.eraTarget < .5) {
    if (!state.history.wheelCollected && playerNearPickup) {
      state.inventory.handwheel = true;
      state.history.wheelCollected = true;
      showToast('已从2047年的闸门机构上拆下手轮；按 B 查看时间锚背包');
      updateHud();
    } else if (playerNearSocket) {
      showToast('2047年的电磁联锁仍在通电；把手轮带到断电的2147年');
    }
    return;
  }

  if (!playerNearSocket) return;
  if (!state.history.wheelInstalled && state.inventory.handwheel) {
    state.inventory.handwheel = false;
    state.history.wheelInstalled = true;
    showToast('2147年操作：从背包取出2047年的手轮并安装到闸门接口');
    updateHud();
  } else if (!state.history.wheelInstalled) {
    showToast('缺少机械手轮；按 Q 到2047年取得它');
  } else if (!state.history.gateOpened) {
    state.history.gateOpened = true;
    showToast('手轮开始转动：配重拉起03号闸门');
    updateHud();
  } else {
    showToast('机械安全卡扣已经锁定，闸门保持开启');
  }
}

function checkHistoryEvents() {
  if (
    state.eraTarget > .5
    && state.history.gateOpened
    && state.gateLift > .82
    && player.x > 12.15
    && !state.exitReached
  ) {
    state.exitReached = true;
    showToast('闸门后是损坏的升降机：现代无法下降，但2047年的电梯仍在运行');
    updateHud();
  }

  if (
    state.eraTarget > .5
    && state.history.experimentShutdown
    && isLowerLevel()
    && player.x > bossTriggerX
    && !state.bossAwake
  ) {
    state.bossAwake = true;
    state.pulse = 1;
    state.boss.projectileTimer = .8;
    showToast('异常采掘构装体苏醒：按 1/2 切换长剑与长矛，按 J 或点击鼠标攻击');
    updateHud();
  }
}

function tryAttack() {
  if (state.inventoryOpen || state.elevatorRiding || state.attackCooldown > 0) return;
  const sword = state.selectedWeapon === 'sword';
  state.attackDuration = sword ? .34 : .46;
  state.attackTimer = state.attackDuration;
  state.attackCooldown = sword ? .4 : .54;
  state.attackHit = false;
}

function clearBossProjectiles() {
  for (const projectile of bossProjectiles) bossProjectileLayer.remove(projectile.mesh);
  bossProjectiles.length = 0;
}

function damagePlayer(amount, knockDirection) {
  if (player.hurtCooldown > 0) return false;
  player.health = Math.max(0, player.health - amount);
  player.hurtCooldown = .82;
  player.vx = knockDirection * 7.5;
  player.vy = 5.2;
  player.grounded = false;
  state.pulse = Math.max(state.pulse, .45);
  const defeated = player.health <= 0;
  if (defeated) {
    player.health = player.maxHealth;
    player.x = 64.2;
    player.y = labGroundY + player.halfH;
    player.vx = 0;
    player.vy = 0;
    state.boss.health = state.boss.maxHealth;
    state.boss.x = 75;
    state.boss.vx = 0;
    clearBossProjectiles();
    showToast('时间锚将你重构在Boss房入口；构装体也恢复了完整状态');
  } else {
    showToast(`受到 ${amount} 点伤害 · 剩余生命 ${Math.ceil(player.health)}`);
  }
  updateHud();
  return defeated;
}

function damageBoss(amount) {
  if (!state.bossAwake || state.boss.defeated || state.eraTarget < .5) return;
  state.boss.health = Math.max(0, state.boss.health - amount);
  state.boss.hitFlash = .16;
  state.boss.vx += player.facing * (state.selectedWeapon === 'sword' ? 1.8 : 2.6);
  if (state.boss.health <= 0) {
    state.boss.defeated = true;
    state.boss.vx = 0;
    state.pulse = 1;
    clearBossProjectiles();
    showToast('异常采掘构装体被击败：Boss房封锁与时间扰动场已经解除');
  }
  updateHud();
}

function spawnBossProjectile() {
  const mesh = new THREE.Group();
  disc(mesh, .3, '#78e5ef', 0, 0, .12, .92, 18);
  ring(mesh, .48, .4, '#b7f9fc', 0, 0, .1, .52, 22);
  mesh.position.set(state.boss.x, labGroundY + 2.75, 2.5);
  bossProjectileLayer.add(mesh);
  const dx = player.x - state.boss.x;
  const dy = player.y - (labGroundY + 2.75);
  const length = Math.max(.001, Math.hypot(dx, dy));
  const speed = 6.2;
  bossProjectiles.push({ mesh, vx: dx / length * speed, vy: dy / length * speed, life: 4.5 });
}

function updateCombat(dt, elapsed) {
  state.attackCooldown = Math.max(0, state.attackCooldown - dt);
  player.hurtCooldown = Math.max(0, player.hurtCooldown - dt);
  state.boss.hitFlash = Math.max(0, state.boss.hitFlash - dt);
  if (state.attackTimer > 0) {
    state.attackTimer = Math.max(0, state.attackTimer - dt);
    const range = state.selectedWeapon === 'sword' ? 1.85 : 3.05;
    const damage = state.selectedWeapon === 'sword' ? 38 : 24;
    const bossAhead = (state.boss.x - player.x) * player.facing > -.35;
    if (!state.attackHit && bossAhead && Math.abs(state.boss.x - player.x) < range && Math.abs(player.y - (labGroundY + player.halfH)) < 1.4) {
      state.attackHit = true;
      damageBoss(damage);
    }
  }

  if (state.bossAwake && !state.boss.defeated && state.eraTarget > .5 && isLowerLevel()) {
    state.boss.decisionTimer -= dt;
    state.boss.chargeTimer = Math.max(0, state.boss.chargeTimer - dt);
    if (state.boss.decisionTimer <= 0) {
      state.boss.decisionTimer = 2.4 + Math.sin(elapsed) * .35;
      state.boss.chargeTimer = .68;
    }
    const targetDirection = Math.sign(player.x - state.boss.x) || 1;
    const bossSpeed = state.boss.chargeTimer > 0 ? 8.2 : 2.5;
    state.boss.vx = THREE.MathUtils.damp(state.boss.vx, targetDirection * bossSpeed, state.boss.chargeTimer > 0 ? 10 : 3.5, dt);
    state.boss.x = THREE.MathUtils.clamp(state.boss.x + state.boss.vx * dt, 64.4, 88.0);

    state.boss.projectileTimer -= dt;
    if (state.boss.projectileTimer <= 0) {
      state.boss.projectileTimer = state.boss.health < state.boss.maxHealth * .5 ? 1.15 : 1.75;
      spawnBossProjectile();
    }
    if (Math.abs(state.boss.x - player.x) < 1.72) {
      damagePlayer(state.boss.chargeTimer > 0 ? 22 : 13, Math.sign(player.x - state.boss.x) || -1);
    }
  }

  for (let index = bossProjectiles.length - 1; index >= 0; index--) {
    const projectile = bossProjectiles[index];
    projectile.life -= dt;
    projectile.mesh.position.x += projectile.vx * dt;
    projectile.mesh.position.y += projectile.vy * dt;
    projectile.mesh.rotation.z += dt * 4.2;
    const hitPlayer = Math.abs(projectile.mesh.position.x - player.x) < .55
      && Math.abs(projectile.mesh.position.y - player.y) < .78;
    if (hitPlayer) {
      bossProjectileLayer.remove(projectile.mesh);
      bossProjectiles.splice(index, 1);
      const playerDefeated = damagePlayer(11, Math.sign(projectile.vx) || 1);
      if (playerDefeated) return;
    } else if (projectile.life <= 0 || projectile.mesh.position.x < 61.5 || projectile.mesh.position.x > 91.5) {
      bossProjectileLayer.remove(projectile.mesh);
      bossProjectiles.splice(index, 1);
    }
  }
}

function updateHud() {
  eventCrate.classList.toggle('active', state.history.wheelCollected);
  eventPlate.classList.toggle('active', state.history.wheelCrossed);
  eventGate.classList.toggle('active', state.history.gateOpened);
  eventElevator.classList.toggle('active', state.history.elevatorUsed);
  eventReactor.classList.toggle('active', state.history.experimentShutdown);
  eventFuture.classList.toggle('active', state.history.futureCleared);
  eventCrate.querySelector('span').textContent = state.history.wheelCollected ? '手轮已从闸门拆下' : '尚未发生';
  eventPlate.querySelector('span').textContent = state.history.wheelCrossed ? '时间锚携带成功' : '等待背包';
  eventGate.querySelector('span').textContent = state.history.gateOpened ? '机械卡扣保持开启' : '等待物品';
  eventElevator.querySelector('span').textContent = state.history.elevatorUsed ? '已抵达地下实验室' : '等待进入';
  eventReactor.querySelector('span').textContent = state.history.experimentShutdown ? '供能已永久切断' : '尚未发生';
  eventFuture.querySelector('span').textContent = state.history.futureCleared ? 'Boss通道已经露出' : '等待改写';

  const lowerLevel = isLowerLevel();
  const beforeLabEntrance = lowerLevel && player.x < labEntranceX - .75;
  if (state.boss.defeated) {
    objective.textContent = 'Boss已击败：异常采掘构装体停止运行，时间扰动场和战斗封锁已经解除';
  } else if (state.bossAwake) {
    objective.textContent = `Boss战：${state.selectedWeapon === 'sword' ? '长剑近身横斩' : '长矛保持距离直刺'} · 按 1/2 切换 · J或鼠标攻击`;
  } else if (state.elevatorRiding) {
    objective.textContent = state.elevatorTargetY === labGroundY
      ? '2047年：升降机正在下降到地下实验室，时间切换暂时受到干扰'
      : '2047年：升降机正在返回地面层，时间切换暂时受到干扰';
  } else if (beforeLabEntrance && state.eraTarget < .5) {
    objective.textContent = '2047年井底：实验室安全门权限锁死。按 Q 去2147年穿过坍塌后的同一入口';
  } else if (beforeLabEntrance) {
    objective.textContent = '2147年井底：实验室安全门已经坍塌，向右穿过缺口进入实验室';
  } else if (lowerLevel && state.eraTarget < .5 && !state.history.experimentShutdown) {
    objective.textContent = '2047年实验室：向右调查时间矿物培养舱，靠近紧急断路杆按 E';
  } else if (lowerLevel && state.eraTarget < .5) {
    objective.textContent = '2047年：培养装置已经永久停机。按 Q 查看这一行为对2147年的影响';
  } else if (lowerLevel && !state.history.experimentShutdown) {
    objective.textContent = '2147年实验室：百年结晶封死Boss通道。按 Q 回2047年关闭培养装置';
  } else if (lowerLevel) {
    objective.textContent = '2147年：异常结晶没有形成，继续向右穿过打开的通道进入Boss房间';
  } else if (state.exitReached && state.eraTarget > .5) {
    objective.textContent = '2147年：电梯已经坠毁。靠近电梯按 Q 回2047年，再按 E 乘坐完整轿厢';
  } else if (state.exitReached) {
    objective.textContent = '2047年：同一部升降机仍在运行。进入右侧轿厢并按 E 下降';
  } else if (state.eraTarget < .5) {
    objective.textContent = state.history.wheelCollected
      ? '2047年：手轮已在时间锚背包中。按 B 查看，按 Q 把它带回2147年'
      : '2047年：闸门主电网在线且电磁锁定。靠近门边手轮按 E 拆下';
  } else if (state.history.gateOpened) {
    objective.textContent = '2147年：闸门已经升起，继续向右调查通往地下实验室的升降机';
  } else if (state.history.wheelInstalled) {
    objective.textContent = '2147年：手轮已经装上。再次按 E 转动手轮并拉起闸门';
  } else if (state.inventory.handwheel) {
    objective.textContent = '2147年：2047年的手轮就在背包里。靠近门边接口按 E 安装';
  } else {
    objective.textContent = '2147年：闸门断电，但原手轮已经锈死。按 Q 查看2047年的同一机构';
  }
  doorStatus.classList.toggle('online', state.eraTarget < .5);
  if (lowerLevel) {
    doorStatus.querySelector('span').textContent = 'CHRONITE CULTURE LAB';
    if (state.eraTarget < .5) {
      doorPower.textContent = state.history.experimentShutdown ? '培养供能：已人工切断' : '培养供能：在线 · 持续生长';
      doorLock.textContent = state.history.experimentShutdown
        ? '历史结果：2147年将不会形成增生结晶'
        : '预测结果：持续运行将封死Boss通道';
    } else {
      doorPower.textContent = state.history.experimentShutdown ? '历史结果：培养实验提前终止' : '历史结果：装置运行至失控';
      doorLock.textContent = state.history.experimentShutdown
        ? 'Boss通道：开放 · 异常结晶未形成'
        : 'Boss通道：封闭 · 结晶严重增生';
    }
  } else if (state.eraTarget < .5) {
    doorStatus.querySelector('span').textContent = '03 GATE CONTROL';
    doorPower.textContent = '主电网：在线 · 供电稳定';
    doorLock.textContent = state.history.wheelCollected
      ? '电磁联锁：锁定 · 应急手轮已拆下'
      : '电磁联锁：锁定 · 手轮无法驱动大门';
  } else {
    doorStatus.querySelector('span').textContent = '03 GATE CONTROL';
    doorPower.textContent = '主电网：离线 · 无法恢复';
    if (state.history.gateOpened) doorLock.textContent = '机械卡扣：已锁定开启位置';
    else if (state.history.wheelInstalled) doorLock.textContent = '电磁联锁：失效 · 新手轮已安装';
    else if (state.inventory.handwheel) doorLock.textContent = '电磁联锁：失效 · 原接口已空置';
    else doorLock.textContent = '电磁联锁：失效 · 原手轮锈死';
  }
  const playerHealthRatio = THREE.MathUtils.clamp(player.health / player.maxHealth, 0, 1);
  playerHealthFill.style.width = `${playerHealthRatio * 100}%`;
  playerHealthCopy.textContent = `${Math.ceil(player.health)} / ${player.maxHealth}`;
  bossHud.classList.toggle('active', state.bossAwake);
  bossHud.setAttribute('aria-hidden', String(!state.bossAwake));
  const bossHealthRatio = THREE.MathUtils.clamp(state.boss.health / state.boss.maxHealth, 0, 1);
  bossHealthFill.style.width = `${bossHealthRatio * 100}%`;
  bossHealthCopy.textContent = state.boss.defeated ? 'DEFEATED' : `${Math.ceil(state.boss.health)} / ${state.boss.maxHealth}`;
  updateInventoryHud();
}

function updateInteractionHint() {
  const lowerLevel = isLowerLevel();
  const nearElevator = Math.abs(player.x - elevatorX) < 1.85;
  const nearLabEntrance = lowerLevel && Math.abs(player.x - labEntranceX) < 1.65;
  const nearReactor = lowerLevel && Math.abs(player.x - reactorControlX) < 1.65;
  const nearBarrier = lowerLevel && Math.abs(player.x - futureBarrierX) < 2.1;
  const nearPastBulkhead = lowerLevel && Math.abs(player.x - 60.2) < 1.9;
  const nearPickup = Math.abs(player.x - handwheelPickupX) < 1.75;
  const nearSocket = Math.abs(player.x - winchSocketX) < 2.0;
  let message = '';

  if (state.inventoryOpen) {
    message = '';
  } else if (nearElevator && state.eraTarget > .5) {
    message = lowerLevel
      ? '井底只有坠毁轿厢 · 按 Q 回2047年，再按 E 乘电梯上升'
      : '空井、断缆、坠毁轿厢 · 按 Q 回到2047年使用完整电梯';
  } else if (nearElevator) {
    if (lowerLevel && state.elevatorAtBottom) message = '按 E 乘2047年的完整轿厢返回地面';
    else if (!lowerLevel && state.elevatorAtBottom) message = '轿厢当前停在地下实验室层';
    else message = '按 E 启动2047年的矿井升降机';
  } else if (nearLabEntrance && state.eraTarget < .5) {
    message = '2047年安全门锁死 · 按 Q 去2147年穿过坍塌入口';
  } else if (nearLabEntrance) {
    message = '2147年安全门已经坍塌 · 向右进入实验室';
  } else if (nearReactor && state.eraTarget < .5) {
    message = state.history.experimentShutdown
      ? '紧急断路杆已锁死 · 培养装置永久停机'
      : '按 E 拉下紧急断路杆 · 改变2147年的结晶结果';
  } else if (nearReactor) {
    message = state.history.experimentShutdown
      ? '装置在2047年已经停机 · 现代只剩惰性矿物'
      : '现代控制台已被吞没 · 必须回2047年操作';
  } else if (nearBarrier && state.eraTarget > .5 && !state.history.experimentShutdown) {
    message = '百年增生结晶封死通道 · 回2047年关闭培养装置';
  } else if (nearPastBulkhead && state.eraTarget < .5) {
    message = '2047年安全门锁定 · 改写实验历史后从2147年进入';
  } else if (state.eraTarget < .5 && nearPickup) {
    message = state.history.wheelCollected
      ? '2047年闸门上的应急手轮已经被你拆下'
      : '按 E 从闸门绞盘上拆下完整手轮并放入背包';
  } else if (state.eraTarget < .5 && nearSocket) {
    message = '2047年的电磁联锁仍在通电，手轮无法绕过权限';
  } else if (state.eraTarget > .5 && nearSocket) {
    if (!state.history.wheelInstalled && state.inventory.handwheel) message = '按 E 从背包取出手轮并安装';
    else if (!state.history.wheelInstalled) message = '2147年的原手轮已经锈死，无法转动';
    else if (!state.history.gateOpened) message = '按 E 转动刚刚安装的机械手轮';
    else message = '配重与安全卡扣正在保持闸门开启';
  }

  interaction.classList.toggle('show', Boolean(message));
  if (message) interaction.querySelector('span').textContent = message;
}

function updateHistoryOutcome(dt, elapsed) {
  const liftTarget = state.history.gateOpened ? 1 : 0;
  state.gateLift = THREE.MathUtils.damp(state.gateLift, liftTarget, 3.4, dt);
  modernGate.door.position.y = 2.58 + state.gateLift * 5.2;
  modernWinch.drum.rotation.z = state.gateLift * Math.PI * 2.2;
  modernInstalledWheel.rotation.z = -state.gateLift * Math.PI * 3.5;

  setLayerOpacity(pastGate.group, 1 - state.era);
  setLayerOpacity(modernGate.group, state.era);
  setLayerOpacity(pastSocket, 1 - state.era);
  setLayerOpacity(modernSocket, state.era);
  setLayerOpacity(pastWinch.group, 1 - state.era);
  setLayerOpacity(modernWinch.group, state.era);
  setLayerOpacity(pastHandwheelPickup, (1 - state.era) * (state.history.wheelCollected ? 0 : 1));
  setLayerOpacity(modernBrokenWheel, state.era * (!state.history.wheelCollected && !state.history.wheelInstalled ? 1 : 0));
  setLayerOpacity(modernInstalledWheel, state.era * (state.history.wheelInstalled ? 1 : 0));

  const pastAmount = 1 - state.era;
  for (let index = 0; index < pastLampMaterials.length; index++) {
    pastLampMaterials[index].opacity = (.62 + Math.sin(elapsed * 3.2 + index * .8) * .18) * pastAmount;
  }
  for (let index = 0; index < pastPowerNodeMaterials.length; index++) {
    const travelingPulse = .18 + Math.max(0, Math.sin(elapsed * 4.6 - index * .72)) * .76;
    pastPowerNodeMaterials[index].opacity = travelingPulse * pastAmount;
  }
  for (let index = 0; index < pastDoorPowerMaterials.length; index++) {
    pastDoorPowerMaterials[index].opacity = (.58 + Math.sin(elapsed * 5.1 + index * .7) * .25) * pastAmount;
  }
  if (pastVentFan) pastVentFan.rotation.z -= dt * 2.15;
  for (let index = 0; index < pastMovingOre.length; index++) {
    pastMovingOre[index].position.x = -11.05 + ((elapsed * .72 + index * .83) % 5.9);
    pastMovingOre[index].position.y = -2.82 + Math.sin(elapsed * 5 + index) * .025;
  }

  if (pastElevatorCar) pastElevatorCar.position.y = state.elevatorY;
  if (pastShutdownLever) {
    const leverTarget = state.history.experimentShutdown ? .55 : -.45;
    pastShutdownLever.rotation.z = THREE.MathUtils.damp(pastShutdownLever.rotation.z, leverTarget, 8, dt);
  }
  if (pastReactorCore) {
    pastReactorCore.rotation.z += dt * (state.history.experimentShutdown ? .16 : 1.25);
    const coreScale = state.history.experimentShutdown ? .62 : 1 + Math.sin(elapsed * 3.2) * .035;
    pastReactorCore.scale.setScalar(THREE.MathUtils.damp(pastReactorCore.scale.x, coreScale, 5, dt));
    setLayerOpacity(pastReactorCore, pastAmount * (state.history.experimentShutdown ? .28 : 1));
  }
  if (modernOvergrowth) {
    const growthAmount = state.era * (state.history.experimentShutdown ? 0 : 1);
    setLayerOpacity(modernOvergrowth, growthAmount);
    const growthPulse = 1 + Math.sin(elapsed * 2.15) * .018;
    modernOvergrowth.scale.set(growthPulse, growthPulse, 1);
  }
  if (modernInertCore) setLayerOpacity(modernInertCore, state.era * (state.history.experimentShutdown ? 1 : 0));
  if (bossBattleBarrier) {
    setLayerOpacity(bossBattleBarrier, state.era * (state.bossAwake && !state.boss.defeated ? 1 : 0));
  }
  if (pastBoss) {
    pastBoss.rotation.z = Math.sin(elapsed * .5) * .012;
  }
  if (modernBoss) {
    const awakeAmount = state.bossAwake && !state.boss.defeated ? 1 : 0;
    modernBoss.position.x = state.boss.x;
    const bossTargetY = state.boss.defeated ? labGroundY + .75 : labGroundY + 2.35 + Math.sin(elapsed * (state.bossAwake ? 2.4 : .7)) * (.16 + awakeAmount * .22);
    modernBoss.position.y = THREE.MathUtils.damp(modernBoss.position.y, bossTargetY, state.boss.defeated ? 2.2 : 8, dt);
    const bossTargetRotation = state.boss.defeated ? -1.12 : Math.sin(elapsed * (state.bossAwake ? 1.8 : .45)) * (.018 + awakeAmount * .035);
    modernBoss.rotation.z = THREE.MathUtils.damp(modernBoss.rotation.z, bossTargetRotation, 5, dt);
    const bossScale = state.boss.defeated ? .82 : (state.bossAwake ? 1.08 + Math.sin(elapsed * 3.4) * .025 : 1);
    modernBoss.scale.setScalar(THREE.MathUtils.damp(modernBoss.scale.x, bossScale, 4.5, dt));
  }
  for (let index = 0; index < bossCoreMaterials.length; index++) {
    const eraAmount = index === 0 ? 1 - state.era : state.era;
    bossCoreMaterials[index].opacity = (.62 + Math.sin(elapsed * (state.bossAwake ? 5.2 : 2.0) + index) * .24) * eraAmount;
  }
  if (bossCoreMaterials[1]) bossCoreMaterials[1].color.set(state.boss.hitFlash > 0 ? '#ffffff' : '#82edf6');

  modernGate.indicator.material.color.set(state.history.gateOpened ? '#83eff6' : '#648990');
  exitGlow.material.opacity = .035 + state.gateLift * .12 + Math.sin(elapsed * 3.1) * .012;
  exitCore.material.opacity = .18 + state.gateLift * .62;
}

function updateVisuals(dt, elapsed) {
  state.era = THREE.MathUtils.damp(state.era, state.eraTarget, 7.5, dt);
  state.pulse = Math.max(0, state.pulse - dt * 1.8);
  backgroundMaterial.uniforms.uEra.value = state.era;
  backgroundMaterial.uniforms.uTime.value = elapsed;
  backgroundMaterial.uniforms.uPulse.value = state.pulse;

  setLayerOpacity(pastLayer, 1 - state.era);
  setLayerOpacity(presentLayer, state.era);
  setLayerOpacity(exitGroup, state.era);

  const groundPast = new THREE.Color('#5d2b25');
  const groundPresent = new THREE.Color('#274048');
  groundMaterial.color.copy(groundPast.lerp(groundPresent, state.era));
  bedrockMaterial.color.copy(new THREE.Color('#351817').lerp(new THREE.Color('#122b31'), state.era));
  deepBedrockMaterial.color.copy(new THREE.Color('#2a1115').lerp(new THREE.Color('#0d242a'), state.era));
  labGroundMaterial.color.copy(new THREE.Color('#663126').lerp(new THREE.Color('#203a40'), state.era));
  strataMaterial.color.copy(new THREE.Color('#9a5136').lerp(new THREE.Color('#3e6870'), state.era));
  undergroundOreMaterial.color.copy(new THREE.Color('#b85d32').lerp(new THREE.Color('#3b7d86'), state.era));
  lowerTunnelMaterial.color.copy(new THREE.Color('#18090c').lerp(new THREE.Color('#081419'), state.era));
  particleMaterial.color.copy(new THREE.Color('#ff9b52').lerp(new THREE.Color('#82d9e5'), state.era));
  particles.rotation.z = Math.sin(elapsed * .08) * .012;

  const playerPast = new THREE.Color('#ffd8ad');
  const playerPresent = new THREE.Color('#dffaff');
  const playerColor = playerPast.lerp(playerPresent, state.era);
  for (const item of playerMesh.userData.bodyMaterials) item.color.copy(playerColor);
  const accentColor = new THREE.Color('#dc8b4d').lerp(new THREE.Color('#75cbd6'), state.era);
  for (const item of playerMesh.userData.accentMaterials) item.color.copy(accentColor);

  const rig = playerMesh.userData.rig;
  const moving = player.grounded ? THREE.MathUtils.clamp(Math.abs(player.vx) / player.speed, 0, 1) : 0;
  player.walkBlend = THREE.MathUtils.damp(player.walkBlend, moving, moving > player.walkBlend ? 12 : 9, dt);
  if (moving > .04) player.walkPhase += dt * (7.2 + Math.abs(player.vx) * .8);
  const gaitFrames = [
    { leftHip: .38, rightHip: -.28, leftKnee: -.04, rightKnee: -.42 },
    { leftHip: .04, rightHip: -.04, leftKnee: -.08, rightKnee: -.14 },
    { leftHip: -.28, rightHip: .38, leftKnee: -.42, rightKnee: -.04 },
    { leftHip: -.04, rightHip: .04, leftKnee: -.14, rightKnee: -.08 },
  ];
  const gaitFrame = Math.floor(((player.walkPhase % (Math.PI * 2)) / (Math.PI * 2)) * 4) % 4;
  const gaitPose = gaitFrames[gaitFrame];
  const stride = (gaitPose.leftHip - gaitPose.rightHip) * .5 * player.walkBlend;
  const armStride = -stride * .72;
  const airborne = player.grounded ? 0 : 1;
  const leftLegTarget = airborne ? -.23 : gaitPose.leftHip * player.walkBlend;
  const rightLegTarget = airborne ? .3 : gaitPose.rightHip * player.walkBlend;
  const leftKneeTarget = airborne ? -.32 : gaitPose.leftKnee * player.walkBlend;
  const rightKneeTarget = airborne ? -.12 : gaitPose.rightKnee * player.walkBlend;
  const leftArmTarget = airborne ? .22 : armStride;
  const rightArmTarget = airborne ? -.34 : -armStride;
  rig.leftLeg.rotation.z = THREE.MathUtils.damp(rig.leftLeg.rotation.z, leftLegTarget, 15, dt);
  rig.rightLeg.rotation.z = THREE.MathUtils.damp(rig.rightLeg.rotation.z, rightLegTarget, 15, dt);
  rig.leftKnee.rotation.z = THREE.MathUtils.damp(rig.leftKnee.rotation.z, leftKneeTarget, 17, dt);
  rig.rightKnee.rotation.z = THREE.MathUtils.damp(rig.rightKnee.rotation.z, rightKneeTarget, 17, dt);
  rig.leftArm.rotation.z = THREE.MathUtils.damp(rig.leftArm.rotation.z, leftArmTarget, 13, dt);
  rig.rightArm.rotation.z = THREE.MathUtils.damp(rig.rightArm.rotation.z, rightArmTarget, 13, dt);

  const stepBounce = Math.abs(Math.sin(player.walkPhase * 2)) * .045 * player.walkBlend;
  rig.bodyRig.position.y = rig.bodyBaseY + stepBounce;
  rig.headRig.position.y = rig.headBaseY + stepBounce * .72;
  rig.leftArm.position.y = rig.leftArmBaseY + stepBounce;
  rig.rightArm.position.y = rig.rightArmBaseY + stepBounce;
  const activeFloorY = isLowerLevel() ? labGroundY : groundY;
  const jumpHeight = Math.max(0, player.y - (activeFloorY + player.halfH));
  rig.shadow.position.y = -.83 - jumpHeight;
  rig.shadow.scale.x = 1 - Math.min(.42, jumpHeight * .12) + player.walkBlend * .06;
  rig.shadow.scale.y = .24 - Math.min(.08, jumpHeight * .018) + Math.sin(player.walkPhase * 2) * .015 * player.walkBlend;
  rig.shadow.material.opacity = .24 - Math.min(.13, jumpHeight * .035);

  const attacking = state.attackTimer > 0;
  const attackProgress = attacking && state.attackDuration > 0
    ? 1 - state.attackTimer / state.attackDuration
    : 0;
  swordMesh.visible = state.selectedWeapon === 'sword';
  spearMesh.visible = state.selectedWeapon === 'spear';
  if (state.selectedWeapon === 'sword') {
    swordMesh.rotation.z = attacking ? -1.02 + attackProgress * 1.92 : -.72;
    swordMesh.position.set(.02, attacking ? .08 : -.08, 0);
    slashMesh.visible = attacking;
    slashMesh.rotation.z = -.25 + attackProgress * .65;
    slashMesh.material.opacity = attacking ? Math.sin(attackProgress * Math.PI) * .3 : 0;
  } else {
    spearMesh.rotation.z = -.16;
    spearMesh.position.set(attacking ? Math.sin(attackProgress * Math.PI) * .92 : 0, attacking ? .04 : -.12, 0);
    slashMesh.visible = false;
  }

  if (player.hurtCooldown > .55) {
    for (const item of playerMesh.userData.bodyMaterials) item.color.set('#ff9b8d');
  }

  playerMesh.position.set(player.x, player.y, 2.2);
  playerMesh.scale.x = player.facing;
  playerMesh.rotation.z = THREE.MathUtils.damp(playerMesh.rotation.z, -player.vx * .008, 12, dt);

  const viewHalfWidth = 9 * (innerWidth / innerHeight);
  let cameraTargetX;
  let cameraTargetY;
  if (state.elevatorRiding) {
    cameraTargetX = elevatorX;
    cameraTargetY = player.y + 3.0;
  } else if (isLowerLevel()) {
    cameraTargetX = THREE.MathUtils.clamp(player.x, 17 + viewHalfWidth, 92 - viewHalfWidth);
    cameraTargetY = labGroundY + 4.15;
  } else {
    cameraTargetX = THREE.MathUtils.clamp(player.x, -17 + viewHalfWidth, 22 - viewHalfWidth);
    cameraTargetY = 0;
  }
  state.cameraX = THREE.MathUtils.damp(state.cameraX, cameraTargetX, state.elevatorRiding ? 3.2 : 5.8, dt);
  state.cameraY = THREE.MathUtils.damp(state.cameraY, cameraTargetY, state.elevatorRiding ? 3.2 : 5.8, dt);
  const cameraShake = state.pulse * Math.sin(elapsed * 58) * .08;
  camera.position.x = state.cameraX + cameraShake;
  camera.position.y = state.cameraY + state.pulse * Math.cos(elapsed * 49) * .035;
  camera.zoom = 1 + state.pulse * .012;
  camera.updateProjectionMatrix();
}

function resize() {
  const aspect = innerWidth / innerHeight;
  const height = 18;
  camera.left = -height * aspect / 2;
  camera.right = height * aspect / 2;
  camera.top = height / 2;
  camera.bottom = -height / 2;
  camera.updateProjectionMatrix();
  renderer.setSize(innerWidth, innerHeight, false);
}

addEventListener('keydown', event => {
  if (!event.repeat && event.code === 'Digit1') selectWeapon('sword');
  if (!event.repeat && event.code === 'Digit2') selectWeapon('spear');
  if (!event.repeat && event.code === 'KeyJ') tryAttack();
  if (!event.repeat && event.code === 'KeyB') toggleInventory();
  if (!event.repeat && event.code === 'Escape' && state.inventoryOpen) setInventoryOpen(false);
  if (!event.repeat && event.code === 'KeyQ') toggleEra();
  if (!event.repeat && event.code === 'KeyR') resetHistory();
  if (!event.repeat && event.code === 'KeyE') handleInteraction();
  if (!event.repeat && (event.code === 'KeyW' || event.code === 'Space')) tryJump();
  keys.add(event.code);
  if (['Space', 'KeyW', 'KeyA', 'KeyB', 'KeyD', 'KeyE', 'KeyJ', 'KeyQ', 'Digit1', 'Digit2'].includes(event.code)) event.preventDefault();
});
addEventListener('pointerdown', event => {
  if (event.button === 0) tryAttack();
});
addEventListener('keyup', event => keys.delete(event.code));
addEventListener('blur', () => keys.clear());
addEventListener('resize', resize);

resize();
updateHud();

function animate() {
  requestAnimationFrame(animate);
  const dt = Math.min(clock.getDelta(), .04);
  const elapsed = clock.elapsedTime;
  updateElevator(dt);
  updateHorizontal(dt);
  updateVertical(dt);
  checkHistoryEvents();
  updateCombat(dt, elapsed);
  updateVisuals(dt, elapsed);
  updateHistoryOutcome(dt, elapsed);
  updateInteractionHint();
  renderer.render(scene, camera);
}

animate();
