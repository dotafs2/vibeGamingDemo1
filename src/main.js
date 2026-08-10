import * as THREE from './vendor/three.module.js';
import { createAssetEditor } from './editor.js';
import { ART_DIRECTION_SPEC, createMineParallaxArt } from './art-direction.js';

const canvas = document.querySelector('#game');
const runtimeParams = new URLSearchParams(location.search);
const lowPowerMode = runtimeParams.has('low-power');
const eraLabel = document.querySelector('#era-label');
const objective = document.querySelector('#objective');
const toast = document.querySelector('#toast');
const timeFlash = document.querySelector('#time-flash');
const interaction = document.querySelector('#interaction');
const eventCrate = document.querySelector('#event-crate');
const eventPlate = document.querySelector('#event-plate');
const eventGate = document.querySelector('#event-gate');
const eventElevator = document.querySelector('#event-elevator');
const eventWorkOrder = document.querySelector('#event-work-order');
const eventReactor = document.querySelector('#event-reactor');
const eventFuture = document.querySelector('#event-future');
const inventoryPanel = document.querySelector('#inventory-panel');
const handwheelSlot = document.querySelector('#handwheel-slot');
const inventoryItemName = document.querySelector('#inventory-item-name');
const inventoryItemStatus = document.querySelector('#inventory-item-status');
const doorStatus = document.querySelector('#door-status');
const doorPower = document.querySelector('#door-power');
const doorLock = document.querySelector('#door-lock');
const hotbarCalibrator = document.querySelector('#hotbar-calibrator');
const hotbarDrill = document.querySelector('#hotbar-drill');
const hotbarHammer = document.querySelector('#hotbar-hammer');
const hotbarWrench = document.querySelector('#hotbar-wrench');
const hotbarWheel = document.querySelector('#hotbar-wheel');
const hotbarBreach = document.querySelector('#hotbar-breach');
const breachKitSlot = document.querySelector('#breach-kit-slot');
const playerHealthFill = document.querySelector('#player-health-fill');
const playerHealthCopy = document.querySelector('#player-health-copy');
const bossHud = document.querySelector('#boss-hud');
const bossHealthFill = document.querySelector('#boss-health-fill');
const bossHealthCopy = document.querySelector('#boss-health-copy');
const npcDialogue = document.querySelector('#npc-dialogue');
const npcDialogueCopy = document.querySelector('#npc-dialogue-copy');

const scene = new THREE.Scene();
const camera = new THREE.OrthographicCamera(-16, 16, 9, -9, 0.1, 100);
camera.position.z = 20;

const renderer = new THREE.WebGLRenderer({ canvas, antialias: true, alpha: false, preserveDrawingBuffer: true });
renderer.setPixelRatio(lowPowerMode ? 1 : Math.min(devicePixelRatio, 2));
renderer.outputColorSpace = THREE.SRGBColorSpace;

const clock = new THREE.Clock();
const keys = new Set();
const groundY = -4.55;
const labGroundY = -22.4;
const elevatorX = 18.25;
const labEntranceX = 21.7;
const maintenanceNpcX = 52.1;
const scanConsoleX = 55.0;
const workOrderX = 40.2;
const bossDoorX = 60.2;
const chargeSocketX = 61.45;
const modernDetonatorX = 58.35;
const bossTriggerX = 65.0;
const bossHomeX = 89.0;
const bossArenaEndX = 116.0;
const bossCoreY = labGroundY + 2.28;
const bossPlatforms = [
  { x: 68.0, width: 5.4, top: labGroundY + 1.55 },
  { x: 73.0, width: 5.4, top: labGroundY + 3.15 },
  { x: 78.0, width: 5.4, top: labGroundY + 4.75 },
  { x: 100.0, width: 5.4, top: labGroundY + 4.75 },
  { x: 105.0, width: 5.4, top: labGroundY + 3.15 },
  { x: 110.0, width: 5.4, top: labGroundY + 1.55 },
];

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
  npcDialogueStep: 0,
  maintenanceScanActive: false,
  lastNpcWarning: -10,
  doorBlast: 0,
  bossAwake: false,
  attackTimer: 0,
  attackDuration: 0,
  attackCooldown: 0,
  equippedWeapon: 'calibrator',
  attackWeapon: 'calibrator',
  attackAimX: 1,
  attackAimY: 0,
  meleeHit: false,
  weaponReadyTimer: 0,
  wrenchInFlight: false,
  wrenchReleasePending: false,
  attackHeld: false,
  boss: {
    maxHealth: 1440,
    health: 1440,
    x: bossHomeX,
    direction: -1,
    patrolMinX: 69.5,
    patrolMaxX: 108.5,
    beamPhase: 'cooldown',
    beamTimer: 10,
    beamOriginX: bossHomeX + 1.12,
    beamOriginY: labGroundY + 3.58,
    beamDirectionX: -1,
    beamDirectionY: 0,
    beamLength: 48,
    beamHit: false,
    laserPhase: 'idle',
    laserTimer: 1.85,
    attackIndex: 0,
    airY: 0,
    jumpStartX: bossHomeX,
    jumpTargetX: bossHomeX,
    drillAngle: Math.PI,
    drillLength: 0,
    drillHit: false,
    dashDirection: -1,
    dashHit: false,
    dashStartX: bossHomeX,
    jumpInvulnerable: false,
    hitFlash: 0,
    defeated: false,
  },
  cameraX: 0,
  cameraY: 0,
  inventoryOpen: false,
  inventory: {
    handwheel: false,
    breachKit: true,
  },
  history: {
    wheelCollected: false,
    wheelCrossed: false,
    wheelInstalled: false,
    gateOpened: false,
    elevatorUsed: false,
    workOrderFound: false,
    bossBriefed: false,
    chargesInstalled: false,
    scanFinalized: false,
    doorBreached: false,
  },
};

const player = {
  x: -10.4,
  y: groundY + .86,
  vx: 0,
  vy: 0,
  halfW: .38,
  halfH: .86,
  standingHalfH: .86,
  crouchHalfH: .5,
  crouching: false,
  crouchBlend: 0,
  dropThroughTimer: 0,
  speed: 6.2,
  jumpSpeed: 10.4,
  grounded: true,
  supportY: groundY,
  facing: 1,
  aimX: 1,
  aimY: 0,
  walkPhase: 0,
  walkBlend: 0,
  maxHealth: 100,
  health: 100,
  hurtCooldown: 0,
};

const pointerAim = {
  initialized: false,
  ndc: new THREE.Vector2(.65, 0),
  world: new THREE.Vector3(),
};

let assetEditor = null;
let artTourEnabled = false;
let artTourStop = null;

const artTourKeyframes = [
  { time: 0, x: -1, y: 0 },
  { time: 5, x: 6, y: 0 },
  { time: 9, x: 18.25, y: 0 },
  { time: 14, x: 18.25, y: labGroundY + 4.15 },
  { time: 19, x: 33, y: labGroundY + 4.15 },
  { time: 25, x: 50, y: labGroundY + 4.15 },
  { time: 30, x: 60, y: labGroundY + 4.15 },
  { time: 39, x: 89, y: labGroundY + 4.15 },
  { time: 45, x: 100, y: labGroundY + 4.15 },
];

function sampleArtTour(elapsed) {
  const duration = artTourKeyframes.at(-1).time;
  const time = elapsed % duration;
  let nextIndex = artTourKeyframes.findIndex(frame => frame.time >= time);
  if (nextIndex <= 0) return artTourKeyframes[0];
  const previous = artTourKeyframes[nextIndex - 1];
  const next = artTourKeyframes[nextIndex];
  const raw = (time - previous.time) / Math.max(.001, next.time - previous.time);
  const eased = raw * raw * (3 - 2 * raw);
  return {
    x: THREE.MathUtils.lerp(previous.x, next.x, eased),
    y: THREE.MathUtils.lerp(previous.y, next.y, eased),
  };
}

function refreshAimDirection() {
  if (!pointerAim.initialized) {
    player.aimX = player.facing;
    player.aimY = 0;
    return;
  }
  pointerAim.world.set(pointerAim.ndc.x, pointerAim.ndc.y, 0).unproject(camera);
  const dx = pointerAim.world.x - player.x;
  const dy = pointerAim.world.y - (player.y + .12);
  const length = Math.hypot(dx, dy);
  if (length < .001) return;
  player.aimX = dx / length;
  player.aimY = dy / length;
  if (Math.abs(dx) > .04) player.facing = dx < 0 ? -1 : 1;
}

function updatePointerAim(event) {
  pointerAim.ndc.set(
    event.clientX / innerWidth * 2 - 1,
    -(event.clientY / innerHeight) * 2 + 1,
  );
  pointerAim.initialized = true;
  refreshAimDirection();
}

const weaponDefinitions = {
  calibrator: {
    name: '小型时相校准器',
    detail: '快捷栏 1 · 鼠标自由瞄准 · 远程检修脉冲 · 24伤害',
    duration: .18,
    cooldown: .3,
  },
  coreDrill: {
    name: '双手脉冲钻',
    detail: '快捷栏 2 · 双手持握 · 按住连续钻击 · 每次32伤害',
    duration: .2,
    cooldown: .22,
    range: 1.72,
    damage: 32,
    activeStart: .12,
    activeEnd: .9,
    minDot: .38,
    melee: true,
  },
  impactHammer: {
    name: '液压挥击锤',
    detail: '快捷栏 3 · 单手与锤柄锁定 · 点击一次完成一段前向圆弧 · 84伤害',
    duration: .32,
    cooldown: .52,
    range: 2.05,
    damage: 84,
    activeStart: .28,
    activeEnd: .78,
    minDot: .18,
    melee: true,
  },
  returnWrench: {
    name: '回弹开口扳手',
    detail: '快捷栏 4 · 脱手后追踪Boss核心 · 命中或超时后自动返回 · 20伤害',
    duration: .36,
    cooldown: .52,
  },
};

function equipWeapon(weaponId) {
  if (!weaponDefinitions[weaponId] || state.attackTimer > 0) return;
  state.equippedWeapon = weaponId;
  state.weaponReadyTimer = 0;
  updateInventoryHud();
  showToast(`已装备${weaponDefinitions[weaponId].name}`);
}

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

const background = new THREE.Mesh(new THREE.PlaneGeometry(180, 70), backgroundMaterial);
background.position.set(46, -10, -12);
scene.add(background);
const mineParallaxArt = createMineParallaxArt({ THREE, scene });

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

const interactionTextureLoader = new THREE.TextureLoader();
const interactionTextureCache = new Map();
function imageRectangle(group, url, width, height, x, y, z = 0, opacity = 1) {
  let texture = interactionTextureCache.get(url);
  if (!texture) {
    texture = interactionTextureLoader.load(url);
    texture.colorSpace = THREE.SRGBColorSpace;
    texture.minFilter = THREE.LinearMipmapLinearFilter;
    texture.magFilter = THREE.LinearFilter;
    interactionTextureCache.set(url, texture);
  }
  const imageMaterial = new THREE.MeshBasicMaterial({
    map: texture,
    transparent: true,
    opacity,
    alphaTest: .01,
    depthWrite: false,
    toneMapped: false,
  });
  imageMaterial.userData.baseOpacity = opacity;
  const mesh = new THREE.Mesh(new THREE.PlaneGeometry(width, height), imageMaterial);
  mesh.position.set(x, y, z);
  mesh.renderOrder = 500;
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

function thickSegment(group, x1, y1, x2, y2, color, thickness = .2, z = .1, opacity = 1) {
  const dx = x2 - x1;
  const dy = y2 - y1;
  const mesh = rectangle(group, Math.hypot(dx, dy), thickness, color, (x1 + x2) * .5, (y1 + y2) * .5, z, opacity);
  mesh.rotation.z = Math.atan2(dy, dx);
  return mesh;
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
let pastMaintenanceNpc = null;
let pastScanCore = null;
let pastScanDoor = null;
let pastBossDoor = null;
let pastChargeSockets = null;
let pastWorkOrder = null;
let modernWorkOrder = null;
let modernBossDoor = null;
let modernDoorRubble = null;
let modernBlastFx = null;
let modernBoss = null;
let pastBoss = null;
let bossBattleBarrier = null;
const bossCoreMaterials = [];
const playerShots = [];

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
  boss.position.set(bossHomeX, labGroundY + .04, -.35);

  // A compact four-legged sampling rig, not a tank: its legs let it traverse uneven ore galleries.
  const legs = [];
  for (const [hipX, footX, phase] of [[-1.15, -2.0, 0], [-.55, -1.15, Math.PI], [.7, 1.2, Math.PI], [1.25, 2.0, 0]]) {
    const leg = new THREE.Group();
    leg.position.set(hipX, 1.65, -.02);
    const localFootX = footX - hipX;
    thickSegment(leg, 0, 0, localFootX * .55, -.68, palette.bossTrim, .24, .02, .94);
    thickSegment(leg, localFootX * .55, -.68, localFootX, -1.55, palette.bossShell, .3, .03, .98);
    disc(leg, .2, palette.bossBody, localFootX * .55, -.68, .08, .98, 18);
    rectangle(leg, .72, .18, palette.bossTrim, localFootX, -1.59, .05, .96).rotation.z = localFootX < 0 ? -.06 : .06;
    leg.userData.phase = phase;
    legs.push(leg);
    boss.add(leg);
  }
  boss.userData.legs = legs;

  polygon(boss, [
    [-1.8, 1.45], [-1.35, 3.18], [-.55, 3.65], [.9, 3.58], [1.85, 2.8],
    [1.78, 1.58], [.9, 1.18], [-.95, 1.2],
  ], palette.bossShell, 0, 0, .04, .98);
  thickSegment(boss, -1.25, 1.55, 1.35, 1.55, palette.bossTrim, .18, .1, .86);
  ring(boss, 1.02, .88, palette.bossTrim, .12, 2.33, .1, .82, 32);

  // The forward cutter still reads as mining equipment, but it is sized for crystal samples rather than tunnelling.
  const cutter = new THREE.Group();
  cutter.position.set(-1.82, 2.33, .16);
  ring(cutter, 1.02, .7, palette.drill, 0, 0, 0, .98, 36);
  ring(cutter, .62, .25, palette.bossTrim, 0, 0, .03, .9, 28);
  disc(cutter, .22, palette.bossBody, 0, 0, .05, .98, 24);
  for (let index = 0; index < 8; index++) {
    const angle = index * Math.PI / 4;
    const tooth = polygon(cutter, [[.78, -.12], [1.22, 0], [.78, .12]], palette.drill, 0, 0, .02, .96);
    tooth.rotation.z = angle;
    segment(cutter, Math.cos(angle) * .28, Math.sin(angle) * .28, Math.cos(angle) * .72, Math.sin(angle) * .72, palette.bossTrim, .72, .08);
  }
  boss.userData.cutter = cutter;
  boss.add(cutter);

  // Rear jets stop the machine before every saw charge, giving the player a readable warning window.
  const dashJets = new THREE.Group();
  dashJets.position.set(1.72, 2.18, .46);
  ring(dashJets, .32, .22, palette.bossTrim, .02, 0, .02, .98, 20);
  polygon(dashJets, [[0, .19], [1.15, 0], [0, -.19]], palette.bossCore, .24, .26, .04, .92);
  polygon(dashJets, [[0, .12], [.78, 0], [0, -.12]], '#efffff', .28, .26, .06, .96);
  polygon(dashJets, [[0, .19], [1.15, 0], [0, -.19]], palette.bossCore, .24, -.26, .04, .92);
  polygon(dashJets, [[0, .12], [.78, 0], [0, -.12]], '#efffff', .28, -.26, .06, .96);
  dashJets.visible = false;
  boss.userData.dashJets = dashJets;
  boss.add(dashJets);

  // Sample channel and rear ore capsule make the mining process legible without a tank-like conveyor chassis.
  const sampleTube = thickSegment(boss, -1.0, 3.2, 1.45, 2.85, palette.bossTrim, .26, .12, .88);
  sampleTube.rotation.z += .01;
  for (let index = 0; index < 4; index++) {
    disc(boss, .12, palette.crystal, -.75 + index * .58, 3.16 - index * .08, .16, ruined ? .58 : .92, 12);
  }
  polygon(boss, [[-.58, .52], [.58, .52], [.75, -.38], [-.75, -.38]], palette.bossBody, 1.55, 2.72, .13, .98);
  polygon(boss, [[0, .52], [-.3, -.32], [.3, -.32]], palette.crystal, 1.55, 2.83, .17, ruined ? .8 : .68);

  // The visible time-crystal regulation core is both the machine's identity and the player's target.
  ring(boss, .82, .58, palette.bossTrim, .18, 2.28, .18, .96, 36);
  disc(boss, .54, palette.bossBody, .18, 2.28, .2, .98, 28);
  const core = polygon(boss, [[0, .58], [-.38, .08], [-.22, -.52], [.22, -.52], [.38, .08]], palette.bossCore, .18, 2.28, .24, ruined ? .98 : .78);
  bossCoreMaterials.push(core.material);
  for (let index = 0; index < 5; index++) {
    const angle = index * Math.PI * 2 / 5;
    disc(boss, .065, palette.bossCore, .18 + Math.cos(angle) * .69, 2.28 + Math.sin(angle) * .69, .25, .88, 12);
  }

  // Telescopic belly drill: the oversized metal bit emerges straight from the hatch.
  // There is deliberately no exposed black piston between hatch and bit.
  const bellyDrill = new THREE.Group();
  bellyDrill.position.set(.12, 1.36, 3.0);
  bellyDrill.rotation.z = -Math.PI * .5;
  const drillHatch = ring(boss, .52, .35, palette.bossTrim, .12, 1.36, .27, .94, 26);
  const drillBit = new THREE.Group();
  drillBit.position.x = .08;
  polygon(drillBit, [[0, .9], [.72, .7], [1.42, .38], [2.05, 0], [1.42, -.38], [.72, -.7], [0, -.9]], palette.drill, 0, 0, .08, .98);
  for (const x of [.18, .58, .98, 1.38]) segment(drillBit, x, -.69 + x * .17, x + .43, .69 - x * .17, palette.bossCore, .78, .24);
  bellyDrill.add(drillBit);
  bellyDrill.userData.bit = drillBit;
  bellyDrill.userData.hatch = drillHatch;
  bellyDrill.visible = false;
  boss.userData.bellyDrill = bellyDrill;
  boss.add(bellyDrill);

  // A swivelling survey emitter handles the ranged part of the attack cycle: telegraphed crystal rays.
  const emitter = new THREE.Group();
  emitter.position.set(.72, 3.55, .2);
  rectangle(emitter, .9, .28, palette.bossTrim, 0, 0, 0, .96);
  ring(emitter, .34, .2, palette.bossCore, .4, 0, .04, .92, 24);
  rectangle(emitter, .16, .62, palette.bossShell, -.28, -.34, -.02, .9);
  boss.userData.emitter = emitter;
  boss.add(emitter);

  // A literal 03 service plate keeps the model name readable inside the world, not only in the HUD.
  rectangle(boss, 1.0, .58, palette.bossBody, 1.13, 1.83, .15, .82);
  ring(boss, .2, .14, palette.bossTrim, .86, 1.83, .2, .92, 20);
  for (const y of [2.0, 1.83, 1.66]) segment(boss, 1.16, y, 1.44, y, palette.bossTrim, .92, .22);
  segment(boss, 1.44, 1.66, 1.44, 2.0, palette.bossTrim, .92, .22);
  if (ruined) {
    for (const [x, y, scale] of [[-1.05, 3.62, .45], [1.5, 3.12, .38], [.15, 3.88, .34]]) {
      polygon(boss, [[0, .8 * scale], [-.34 * scale, 0], [.34 * scale, 0]], palette.crystal, x, y, .16, .9);
    }
    modernBoss = boss;
  } else {
    rectangle(boss, 4.1, .12, palette.cable, 0, 4.55, -.1, .6);
    segment(boss, -1.35, 4.55, -1.35, 3.48, palette.cable, .65, -.05);
    segment(boss, 1.35, 4.55, 1.35, 3.35, palette.cable, .65, -.05);
    pastBoss = boss;
  }
  group.add(boss);
}

function createMaintenanceNpc(group, palette) {
  const npc = new THREE.Group();
  npc.position.set(maintenanceNpcX, labGroundY + .92, -.2);
  const shadow = new THREE.Mesh(new THREE.CircleGeometry(.48, 24), material('#14090a', .34));
  shadow.scale.y = .22;
  shadow.position.set(0, -.88, -.2);
  npc.add(shadow);
  rectangle(npc, .72, .92, '#e08a4f', 0, -.05, .04, .96);
  rectangle(npc, .78, .18, '#ffd184', 0, .34, .08, .95);
  disc(npc, .35, '#f4c18f', 0, .69, .08, .98, 24);
  rectangle(npc, .68, .16, palette.trim, 0, .91, .1, .96);
  rectangle(npc, .18, .64, '#d47743', -.47, -.02, .02, .94);
  rectangle(npc, .18, .64, '#d47743', .47, -.02, .02, .94);
  rectangle(npc, .22, .65, '#75402e', -.2, -.79, .02, .96);
  rectangle(npc, .22, .65, '#75402e', .2, -.79, .02, .96);
  rectangle(npc, .12, .12, palette.lamp, .2, .02, .12, .96);
  pastMaintenanceNpc = npc;
  group.add(npc);
}

function buildExpandedMine(group, palette, ruined) {
  // Runtime interactables live above the production panoramas. Their world
  // positions and gameplay references are unchanged; only visual depth moves.
  const interactionLayer = new THREE.Group();
  interactionLayer.name = ruined ? 'interactive-overlay-2147' : 'interactive-overlay-2047';
  interactionLayer.position.z = 4;
  group.add(interactionLayer);

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
    car.position.set(elevatorX, groundY, 0);
    rectangle(car, 3.0, .24, palette.trim, 0, .04, 0, .96);
    rectangle(car, .18, 3.7, palette.structure, -1.42, 1.85, 0, .9);
    rectangle(car, .18, 3.7, palette.structure, 1.42, 1.85, 0, .9);
    rectangle(car, 3.0, .24, palette.trim, 0, 3.62, 0, .92);
    rectangle(car, 2.5, .18, palette.cage, 0, 1.65, -.02, .34);
    for (const x of [-1.0, -.5, 0, .5, 1.0]) segment(car, x, .2, x, 3.45, palette.cage, .42, .04);
    rectangle(car, .42, .8, palette.panel, 1.08, 1.3, .08, .9);
    disc(car, .08, palette.lamp, 1.08, 1.48, .12, .95, 16);
    pastElevatorCar = car;
    interactionLayer.add(car);
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
    wreck.position.set(elevatorX, labGroundY + .35, 0);
    wreck.rotation.z = -.12;
    rectangle(wreck, 3.0, .3, palette.trim, 0, .05, 0, .58);
    rectangle(wreck, .22, 2.2, palette.structure, -1.28, 1.0, 0, .45);
    rectangle(wreck, .22, 1.8, palette.structure, 1.15, .8, 0, .38);
    rectangle(wreck, 2.4, .18, palette.cage, 0, 1.5, .02, .28);
    interactionLayer.add(wreck);
  }

  // The laboratory leads into a tall, extended mining-machine test chamber.
  rectangle(group, 39.5, 7.5, palette.labWall, 40.0, labGroundY + 3.55, -4.2, ruined ? .75 : .96);
  rectangle(group, 55.0, 9.1, palette.bossRoom, 88.6, labGroundY + 4.35, -4.35, ruined ? .78 : .94);
  rectangle(group, 96.5, .28, palette.trim, 67.4, labGroundY + 7.35, -2.9, ruined ? .34 : .76);
  rectangle(group, 96.5, .34, palette.floor, 67.4, labGroundY - .16, -2.7, ruined ? .7 : .95);
  for (const x of [21.0, 29.0, 37.0, 45.0, 53.0, 59.2, 62.0, 70.0, 78.0, 88.0, 98.0, 108.0, 115.4]) {
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
    imageRectangle(
      interactionLayer,
      '/editor/assets/scene-gate/v1-past.png',
      4.65,
      6.15,
      labEntranceX,
      labGroundY + 3.08,
      0,
    );
  } else {
    rectangle(group, 1.55, 6.1, '#07161b', labEntranceX, labGroundY + 3.1, -1.02, .98);
    const fallenDoor = rectangle(group, 1.5, 4.2, palette.bulkhead, labEntranceX + .72, labGroundY + .95, -.78, .58);
    fallenDoor.rotation.z = -1.17;
    segment(group, labEntranceX - .55, labGroundY + 5.8, labEntranceX + .35, labGroundY + 4.9, palette.cable, .46, -.65);
    segment(group, labEntranceX + .35, labGroundY + 4.9, labEntranceX + .05, labGroundY + 3.85, palette.cable, .34, -.65);
    const collapsedEntrance = imageRectangle(
      interactionLayer,
      '/editor/assets/scene-gate/v1-present.png',
      4.35,
      5.15,
      labEntranceX + .72,
      labGroundY + 1.05,
      0,
      .9,
    );
    collapsedEntrance.rotation.z = -1.17;
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

  // Maintenance archive beside the time-observation ring: information crosses time, not the old paper itself.
  rectangle(group, 2.5, 1.05, palette.panel, workOrderX, labGroundY + .62, -1.15, ruined ? .58 : .92);
  rectangle(group, .16, 1.15, palette.structure, workOrderX - .9, labGroundY + .02, -1.1, ruined ? .42 : .82);
  rectangle(group, .16, 1.15, palette.structure, workOrderX + .9, labGroundY + .02, -1.1, ruined ? .42 : .82);
  if (!ruined) {
    const order = new THREE.Group();
    rectangle(order, 1.05, .72, '#f1c59a', workOrderX, labGroundY + 1.18, -.72, .96);
    for (let row = 0; row < 4; row++) rectangle(order, .72 - row * .06, .045, '#6f3728', workOrderX, labGroundY + 1.38 - row * .14, -.62, .72);
    rectangle(order, .42, .1, palette.lever, workOrderX, labGroundY + 1.0, -.6, .9);
    pastWorkOrder = order;
    interactionLayer.add(order);
  } else {
    const archive = new THREE.Group();
    rectangle(archive, 1.18, .78, '#0b1b20', workOrderX, labGroundY + 1.2, -.7, .96);
    rectangle(archive, .92, .5, palette.glass, workOrderX, labGroundY + 1.2, -.58, .84);
    for (let row = 0; row < 3; row++) rectangle(archive, .62 - row * .08, .045, palette.archGlow, workOrderX, labGroundY + 1.34 - row * .14, -.46, .82);
    disc(archive, .09, palette.lamp, workOrderX + .43, labGroundY + .93, -.44, .9, 14);
    modernWorkOrder = archive;
    interactionLayer.add(archive);
  }

  // Final-maintenance bay and shielded observation booth.
  rectangle(group, 10.8, 5.5, palette.reactorFrame, 50.7, labGroundY + 3.15, -1.7, ruined ? .46 : .9);
  rectangle(group, 3.5, 3.8, palette.glass, 48.3, labGroundY + 3.15, -1.42, ruined ? .2 : .46);
  rectangle(interactionLayer, 1.35, 1.5, palette.panel, scanConsoleX, labGroundY + 1.25, -.9, ruined ? .46 : .94);
  ring(interactionLayer, .48, .37, palette.trim, scanConsoleX, labGroundY + 1.55, -.65, ruined ? .38 : .86, 28);
  for (let index = 0; index < 3; index++) {
    disc(interactionLayer, .075, ruined ? palette.deadLamp : palette.lamp, scanConsoleX - .34 + index * .34, labGroundY + .82, -.62, ruined ? .34 : .9, 14);
  }
  if (!ruined) {
    createMaintenanceNpc(interactionLayer, palette);
    const scanCore = new THREE.Group();
    scanCore.position.set(50.7, labGroundY + 3.25, -.95);
    ring(scanCore, 1.25, 1.05, palette.archGlow, 0, 0, 0, .74, 42);
    for (let index = 0; index < 6; index++) {
      const angle = index * Math.PI / 3;
      disc(scanCore, .1, palette.lamp, Math.cos(angle) * 1.15, Math.sin(angle) * 1.15, .04, .92, 14);
    }
    pastScanCore = scanCore;
    interactionLayer.add(scanCore);
    const boothShutter = new THREE.Group();
    rectangle(boothShutter, 3.25, 2.55, palette.bulkhead, 48.3, labGroundY + 6.2, .32, .72);
    for (let x = 47.0; x <= 49.6; x += .52) segment(boothShutter, x, labGroundY + 5.0, x, labGroundY + 7.35, palette.trim, .48, .4);
    pastScanDoor = boothShutter;
    interactionLayer.add(boothShutter);
  } else {
    segment(group, 46.8, labGroundY + 5.4, 49.1, labGroundY + 3.7, palette.cable, .42, -.8);
    segment(group, 49.1, labGroundY + 3.7, 47.5, labGroundY + 1.2, palette.cable, .32, -.8);
    for (const [x, y, rotation] of [[47.2, .55, -.18], [50.1, .35, .25], [54.0, .42, -.12]]) {
      const debris = rectangle(group, 1.05, .32, palette.structure, x, labGroundY + y, -.7, .5);
      debris.rotation.z = rotation;
    }
  }

  // Boss-room blast door: open for the final 2047 scan, fused into a wall by 2147.
  rectangle(group, .28, 7.1, palette.trim, 59.2, labGroundY + 3.5, -1.2, .92);
  rectangle(group, .28, 7.1, palette.trim, 61.2, labGroundY + 3.5, -1.2, .92);
  rectangle(group, 2.3, .28, palette.trim, 60.2, labGroundY + 7.0, -1.15, .92);
  if (!ruined) {
    const door = new THREE.Group();
    rectangle(door, 1.55, 6.35, palette.bulkhead, bossDoorX, labGroundY + 3.25, -1.0, .96);
    for (let y = labGroundY + .75; y <= labGroundY + 5.85; y += 1.02) {
      segment(door, bossDoorX - .62, y, bossDoorX + .62, y, palette.trim, .46, -.7);
    }
    imageRectangle(
      door,
      '/editor/assets/scene-gate/v1-past.png',
      4.85,
      6.45,
      bossDoorX,
      labGroundY + 3.22,
      0,
    );
    pastBossDoor = door;
    interactionLayer.add(door);

    // This is an ordinary inner lock rail. The player later mills a hidden groove into it;
    // there is deliberately no pre-existing “explosive slot” in the official door design.
    const sockets = new THREE.Group();
    rectangle(sockets, .32, 4.7, palette.structure, bossDoorX + .73, labGroundY + 3.22, -.58, .82);
    rectangle(sockets, .17, 3.25, palette.bulkhead, bossDoorX + .74, labGroundY + 3.22, -.48, .98);
    for (const y of [labGroundY + 1.15, labGroundY + 2.2, labGroundY + 3.25, labGroundY + 4.3, labGroundY + 5.35]) {
      disc(sockets, .075, palette.trim, bossDoorX + .74, y, -.4, .82, 14);
    }
    const concealedTool = rectangle(sockets, .12, 2.75, palette.archGlow, bossDoorX + .74, labGroundY + 3.22, -.34, .78);
    concealedTool.userData.baseOpacity = .78;
    sockets.userData.concealedTool = concealedTool;
    pastChargeSockets = sockets;
    interactionLayer.add(sockets);

    // Powered safety line: the player can reach the inner lock rail but not the sealed machine bay.
    rectangle(group, .1, 6.5, palette.archGlow, 63.1, labGroundY + 3.4, -.7, .42);
    for (let y = labGroundY + .4; y < labGroundY + 6.6; y += .5) disc(group, .065, palette.lamp, 63.1, y, -.55, .76, 12);
    for (let index = 0; index < 5; index++) {
      const stripe = rectangle(group, .34, .12, index % 2 ? '#321819' : palette.lever, 62.45 + index * .32, labGroundY + .06, -.42, .96);
      stripe.rotation.z = -.45;
    }
    rectangle(group, 1.55, .52, palette.panel, 63.15, labGroundY + 5.75, -.45, .92);
    rectangle(group, 1.05, .06, palette.lever, 63.15, labGroundY + 5.88, -.34, .9);
    rectangle(group, .78, .06, palette.lever, 63.15, labGroundY + 5.68, -.34, .72);
  }
  if (ruined) {
    rectangle(interactionLayer, 1.0, 1.45, palette.panel, modernDetonatorX, labGroundY + 1.18, -.85, .56);
    const sealedDoor = new THREE.Group();
    sealedDoor.position.set(bossDoorX, labGroundY + .1, 0);
    rectangle(sealedDoor, 1.72, 6.45, palette.bulkhead, 0, 3.23, -1.0, .98);
    for (let y = .7; y < 6.0; y += 1.05) segment(sealedDoor, -.72, y, .72, y, palette.structure, .5, -.7);
    segment(sealedDoor, -.62, 5.6, .42, 4.45, palette.crack, .68, -.56);
    segment(sealedDoor, .42, 4.45, -.25, 3.1, palette.crack, .58, -.56);
    segment(sealedDoor, -.25, 3.1, .55, 1.75, palette.crack, .5, -.56);
    imageRectangle(
      sealedDoor,
      '/editor/assets/scene-gate/v1-present.png',
      4.85,
      6.45,
      0,
      3.23,
      0,
    );
    modernBossDoor = sealedDoor;
    interactionLayer.add(sealedDoor);

    const rubble = new THREE.Group();
    for (const [x, width, height, rotation] of [[59.45, .8, .38, -.2], [60.2, 1.05, .45, .14], [61.0, .72, .32, -.12], [61.75, .65, .27, .24]]) {
      const chunk = rectangle(rubble, width, height, palette.bulkhead, x, labGroundY + height * .5, -.55, .82);
      chunk.rotation.z = rotation;
    }
    modernDoorRubble = rubble;
    interactionLayer.add(rubble);

    const blastFx = new THREE.Group();
    blastFx.position.set(bossDoorX, labGroundY + 3.25, 0);
    ring(blastFx, 1.1, .82, palette.archGlow, 0, 0, .15, .85, 36);
    ring(blastFx, 1.85, 1.6, palette.lamp, 0, 0, .14, .48, 36);
    modernBlastFx = blastFx;
    interactionLayer.add(blastFx);

    const battleBarrier = new THREE.Group();
    for (let y = labGroundY + .35; y <= labGroundY + 6.6; y += .48) {
      disc(battleBarrier, .075, palette.archGlow, 62.45, y, -.62, .82, 14);
    }
    rectangle(battleBarrier, .09, 6.55, palette.archGlow, 62.45, labGroundY + 3.45, -.68, .34);
    bossBattleBarrier = battleBarrier;
    interactionLayer.add(battleBarrier);
  }
  // Symmetrical maintenance stairs: readable jump routes inspired by classic multi-level boss rooms.
  for (const platform of bossPlatforms) {
    rectangle(group, platform.width, .28, palette.platform, platform.x, platform.top - .14, -1.0, ruined ? .72 : .92);
    rectangle(group, platform.width - .35, .1, palette.trim, platform.x, platform.top + .03, -.86, ruined ? .5 : .84);
    const supportHeight = Math.max(.5, platform.top - labGroundY);
    for (const offset of [-platform.width * .36, platform.width * .36]) {
      rectangle(group, .16, supportHeight, palette.structure, platform.x + offset, labGroundY + supportHeight * .5, -1.2, ruined ? .4 : .68);
      segment(group, platform.x + offset, labGroundY + .1, platform.x - offset * .42, platform.top - .18, palette.structure, ruined ? .3 : .58, -1.15);
    }
  }

  // Far rock face, ore seams and extraction pipes make the arena part of a mine rather than an empty box.
  rectangle(group, 1.0, 8.4, palette.shaftRock, 115.45, labGroundY + 4.05, -2.1, .96);
  for (const [x1, y1, x2, y2] of [
    [112.9, 6.7, 115.4, 5.8], [113.1, 5.0, 115.4, 4.2], [113.0, 2.9, 115.4, 2.1],
  ]) segment(group, x1, labGroundY + y1, x2, labGroundY + y2, palette.crystal, ruined ? .38 : .68, -1.75);
  for (const y of [1.3, 3.1, 5.1]) {
    segment(group, 63.2, labGroundY + y, 114.8, labGroundY + y + .15, palette.cable, ruined ? .22 : .42, -2.0);
  }
  createLabBoss(group, palette, ruined);

  const lampXs = [23.0, 28.5, 34.0, 40.0, 46.0, 52.0, 56.0, 65.0, 72.0, 79.0, 88.0, 97.0, 106.0, 114.0];
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
    bossCore: '#ffb65f', bossTrim: '#d67a45', drill: '#d98a54', growth: '#ff8f48', growthLine: '#ffb05b', crack: '#e7884e',
  }, false);
}

function buildPresentExpansion() {
  buildExpandedMine(presentLayer, {
    surfaceRoom: '#10252b', trim: '#5f9da6', structure: '#456e75', panel: '#142e34', lamp: '#82d9e5',
    deadLamp: '#526d72', shaftRock: '#09191e', depthMark: '#4e7a81', cable: '#4d7379', cage: '#42666c',
    labWall: '#10272d', bossRoom: '#0b1d22', floor: '#203a40', glass: '#30545b', crystal: '#4c8d96',
    arch: '#4f858d', archGlow: '#75d6e2', reactorFrame: '#17343b', reactorCore: '#5ecbd8', lever: '#79c9d2',
    inertCore: '#54767b', bulkhead: '#17343a', platform: '#365d64', bossShell: '#4e858d', bossBody: '#142b31',
    bossCore: '#82edf6', bossTrim: '#70bdc6', drill: '#6ba9b1', growth: '#4fb4c1', growthLine: '#70d9e4', crack: '#6ba9b1',
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
const deepBedrock = new THREE.Mesh(new THREE.PlaneGeometry(104, 7.5), deepBedrockMaterial);
deepBedrock.position.set(66, labGroundY - 3.9, -5.8);
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
const labGround = new THREE.Mesh(new THREE.PlaneGeometry(102, .6), labGroundMaterial);
labGround.position.set(67, labGroundY - .3, .18);
commonLayer.add(labGround);
for (let x = 16.2; x <= 117.0; x += 1.1) {
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
    shadowBaseY: shadow.position.y,
    leftLegBaseY: leftLeg.position.y,
    rightLegBaseY: rightLeg.position.y,
    leftArmBaseY: leftArm.position.y,
    rightArmBaseY: rightArm.position.y,
  };
  return group;
}

const playerMesh = createPlayer();
playerMesh.userData.editorExcludeFromCapture = true;
scene.add(playerMesh);

function createCalibrator() {
  const group = new THREE.Group();
  rectangle(group, .54, .17, '#284d55', .27, 0, .2, .98);
  rectangle(group, .31, .09, '#8debf1', .33, .015, .22, .88);
  polygon(group, [[0, .1], [.2, .065], [.2, -.065], [0, -.1]], '#d9f8fb', .54, 0, .24, .98);
  rectangle(group, .13, .28, '#96613c', .14, -.17, .18, .98).rotation.z = -.18;
  ring(group, .105, .06, '#8debf1', .5, 0, .26, .92, 18);
  return group;
}

function createCoreDrill() {
  const group = new THREE.Group();
  rectangle(group, .58, .32, '#294d54', .27, 0, .2, .98);
  rectangle(group, .38, .18, '#62c7d0', .29, 0, .22, .82);
  ring(group, .17, .09, '#a9f4f7', .56, 0, .25, .92, 22);
  rectangle(group, .14, .34, '#8a5b3d', .04, -.22, .18, .98).rotation.z = -.08;
  rectangle(group, .14, .34, '#8a5b3d', .4, -.22, .18, .98).rotation.z = .08;
  const rearHand = disc(group, .105, '#dffaff', .04, -.36, .3, .98, 14);
  const frontHand = disc(group, .105, '#dffaff', .4, -.36, .3, .98, 14);
  playerMesh.userData.bodyMaterials.push(rearHand.material, frontHand.material);
  const bit = new THREE.Group();
  bit.position.x = .68;
  polygon(bit, [[0, .15], [.34, .095], [.52, 0], [.34, -.095], [0, -.15]], '#c8fbff', 0, 0, .26, .98);
  const flutes = [];
  for (const x of [.1, .24, .38]) flutes.push(segment(bit, x, -.12, x + .1, .12, '#55bdc7', .85, .28));
  bit.userData.flutes = flutes;
  group.add(bit);
  group.userData.bit = bit;
  return group;
}

function createImpactHammer() {
  const group = new THREE.Group();
  rectangle(group, 1.02, .14, '#8d5b3c', .49, 0, .19, .98);
  ring(group, .11, .055, '#77dbe3', .17, 0, .24, .9, 18);
  rectangle(group, .46, .62, '#294d54', 1.02, 0, .22, .98);
  rectangle(group, .28, .52, '#64c7d0', .98, 0, .24, .78);
  rectangle(group, .15, .66, '#bceff2', 1.23, 0, .25, .86);
  return group;
}

function createReturnWrench() {
  const group = new THREE.Group();
  rectangle(group, .68, .12, '#66c6ce', .29, 0, .22, .98);
  ring(group, .16, .075, '#bff8fa', -.06, 0, .25, .96, 20);
  polygon(group, [[-.03, .08], [.25, .27], [.39, .12], [.22, 0], [.39, -.12], [.25, -.27], [-.03, -.08]], '#bff8fa', .64, 0, .26, .98);
  polygon(group, [[0, .075], [.19, .17], [.28, .08], [.16, 0], [.28, -.08], [.19, -.17], [0, -.075]], '#16343b', .69, 0, .28, 1);
  return group;
}

function createDrillHarness() {
  const group = new THREE.Group();
  rectangle(group, .76, .28, '#263f45', .22, 0, .03, .98);
  ring(group, .16, .08, '#65cbd2', .52, 0, .05, .82, 18);
  polygon(group, [[0, .14], [.42, .08], [.62, 0], [.42, -.08], [0, -.14]], '#8ddce2', .65, 0, .04, .76);
  return group;
}

const weaponRig = new THREE.Group();
weaponRig.position.set(0, -.39, .45);
const calibratorMesh = createCalibrator();
const coreDrillMesh = createCoreDrill();
const impactHammerMesh = createImpactHammer();
const returnWrenchMesh = createReturnWrench();
weaponRig.add(calibratorMesh, coreDrillMesh, impactHammerMesh, returnWrenchMesh);
playerMesh.userData.rig.rightArm.add(weaponRig);
const muzzleFlash = polygon(weaponRig, [[0, .11], [.22, 0], [0, -.11]], '#c8fbff', .75, 0, .32, .9);
muzzleFlash.visible = false;

// Selected weapons are visible on the body while idle, then move into the hand only during combat readiness.
const storageRig = new THREE.Group();
playerMesh.add(storageRig);
const holsteredCalibrator = createCalibrator();
holsteredCalibrator.scale.setScalar(.78);
holsteredCalibrator.position.set(.26, -.36, .04);
holsteredCalibrator.rotation.z = -1.38;
storageRig.add(holsteredCalibrator);
const stowedDrill = createDrillHarness();
stowedDrill.scale.setScalar(.78);
stowedDrill.position.set(-.28, .02, -.33);
stowedDrill.rotation.z = .95;
storageRig.add(stowedDrill);
const stowedHammer = createImpactHammer();
stowedHammer.scale.setScalar(.78);
stowedHammer.position.set(-.22, .02, -.34);
stowedHammer.rotation.z = .92;
storageRig.add(stowedHammer);
const holsteredWrench = createReturnWrench();
holsteredWrench.scale.setScalar(.7);
holsteredWrench.position.set(.28, -.4, .05);
holsteredWrench.rotation.z = -1.12;
storageRig.add(holsteredWrench);

// The player owns the absolute foreground queue. Re-apply it before every
// render so editor assets, transparent sorting and later-added equipment can
// never cover the character.
const PLAYER_FOREGROUND_Z = 12;
const PLAYER_RENDER_ORDER = 100000;
function enforcePlayerForeground() {
  playerMesh.position.z = PLAYER_FOREGROUND_Z;
  playerMesh.renderOrder = PLAYER_RENDER_ORDER;
  playerMesh.traverse(object => {
    object.renderOrder = PLAYER_RENDER_ORDER;
    if (!object.material) return;
    const materials = Array.isArray(object.material) ? object.material : [object.material];
    for (const item of materials) {
      if (item.depthTest || item.depthWrite) {
        item.depthTest = false;
        item.depthWrite = false;
        item.needsUpdate = true;
      }
    }
  });
}
enforcePlayerForeground();

const playerShotLayer = new THREE.Group();
playerShotLayer.userData.editorExcludeFromCapture = true;
const PLAYER_PROJECTILE_Z = 11;
const PLAYER_PROJECTILE_RENDER_ORDER = 90000;
playerShotLayer.renderOrder = PLAYER_PROJECTILE_RENDER_ORDER;
scene.add(playerShotLayer);

function enforceProjectileForeground(object) {
  object.position.z = PLAYER_PROJECTILE_Z;
  object.renderOrder = PLAYER_PROJECTILE_RENDER_ORDER;
  object.traverse(child => {
    child.renderOrder = PLAYER_PROJECTILE_RENDER_ORDER;
    if (!child.material) return;
    const materials = Array.isArray(child.material) ? child.material : [child.material];
    for (const item of materials) {
      if (item.depthTest || item.depthWrite) {
        item.depthTest = false;
        item.depthWrite = false;
        item.needsUpdate = true;
      }
    }
  });
}

const bossBeamLayer = new THREE.Group();
bossBeamLayer.userData.editorExcludeFromCapture = true;
const beamTelegraphMesh = new THREE.Mesh(new THREE.PlaneGeometry(1, 1), material('#82edf6', .24));
const beamGlowMesh = new THREE.Mesh(new THREE.PlaneGeometry(1, 1), material('#6fd9e6', .55));
const beamCoreMesh = new THREE.Mesh(new THREE.PlaneGeometry(1, 1), material('#e9ffff', .96));
beamTelegraphMesh.visible = false;
beamGlowMesh.visible = false;
beamCoreMesh.visible = false;
bossBeamLayer.add(beamTelegraphMesh, beamGlowMesh, beamCoreMesh);
scene.add(bossBeamLayer);

const bossAttackLayer = new THREE.Group();
bossAttackLayer.userData.editorExcludeFromCapture = true;
const bossShockwaves = [];
const bossLaserShots = [];
scene.add(bossAttackLayer);

const particleCount = 360;
const particlePositions = new Float32Array(particleCount * 3);
for (let index = 0; index < particleCount; index++) {
  particlePositions[index * 3] = THREE.MathUtils.randFloat(-17, 118);
  particlePositions[index * 3 + 1] = THREE.MathUtils.randFloat(-27, 7);
  particlePositions[index * 3 + 2] = THREE.MathUtils.randFloat(-5, 4);
}
const particleGeometry = new THREE.BufferGeometry();
particleGeometry.setAttribute('position', new THREE.BufferAttribute(particlePositions, 3));
const particleMaterial = new THREE.PointsMaterial({ color: '#82d9e5', size: .055, transparent: true, opacity: .32 });
const particles = new THREE.Points(particleGeometry, particleMaterial);
particles.userData.editorExcludeFromCapture = true;
scene.add(particles);

function showToast(message) {
  toast.textContent = message;
  toast.classList.add('show');
  clearTimeout(showToast.timer);
  showToast.timer = setTimeout(() => toast.classList.remove('show'), 1900);
}

const maintenanceDialogueLines = [
  '工号7C-113，工单M-03-7721：03号防爆门锁轨与密封检修。身份和任务都对上了。',
  '门后是03型时晶采掘机“掘脉者”，负责沿矿脉行走、切取时晶样本。它今天做最后一次断能封存，之后舱门会灌胶，不再启封。',
  '终检开始后，人员会撤进隔离观察室，防爆门保持开启。你只需要检查门框内侧锁轨和密封条，不要碰封存舱设备。',
  '维修完成就回控制台结束测试。我只验收门体开闭、锁止和密封数据，合格后立即封门。',
];

function closeNpcDialogue() {
  npcDialogue.classList.remove('open');
  npcDialogue.setAttribute('aria-hidden', 'true');
}

function advanceNpcDialogue() {
  if (state.npcDialogueStep >= maintenanceDialogueLines.length) {
    closeNpcDialogue();
    return;
  }
  npcDialogueCopy.textContent = maintenanceDialogueLines[state.npcDialogueStep];
  state.npcDialogueStep += 1;
  npcDialogue.classList.add('open');
  npcDialogue.setAttribute('aria-hidden', 'false');
  if (state.npcDialogueStep === maintenanceDialogueLines.length) {
    state.history.bossBriefed = true;
    showToast('工号与工单核验通过：到右侧控制台按 E 启动防爆门终检');
    updateHud();
  }
}

function flashTime() {
  timeFlash.classList.remove('active');
  void timeFlash.offsetWidth;
  timeFlash.classList.add('active');
}

function updateInventoryHud() {
  const hasWheel = state.inventory.handwheel;
  const hasBreachKit = state.inventory.breachKit;
  handwheelSlot.classList.toggle('empty', !hasWheel);
  hotbarWheel.classList.toggle('empty', !hasWheel);
  breachKitSlot.classList.toggle('empty', !hasBreachKit);
  hotbarBreach.classList.toggle('empty', !hasBreachKit);
  hotbarCalibrator.classList.toggle('selected', state.equippedWeapon === 'calibrator');
  hotbarDrill.classList.toggle('selected', state.equippedWeapon === 'coreDrill');
  hotbarHammer.classList.toggle('selected', state.equippedWeapon === 'impactHammer');
  hotbarWrench.classList.toggle('selected', state.equippedWeapon === 'returnWrench');
  const weapon = weaponDefinitions[state.equippedWeapon];
  inventoryItemName.textContent = weapon.name;
  inventoryItemStatus.textContent = `${weapon.detail} · J / 左键攻击`;
}

function setInventoryOpen(open) {
  // Q closes the inventory defensively before an era switch. When it is
  // already closed, do not clear held movement keys: clearing KeyA/KeyD here
  // creates a visible pause until the browser emits its next key-repeat event.
  if (state.inventoryOpen === open) return;
  state.inventoryOpen = open;
  inventoryPanel.classList.toggle('open', open);
  inventoryPanel.setAttribute('aria-hidden', String(!open));
  keys.clear();
  state.attackHeld = false;
  updateInventoryHud();
}

function toggleInventory() {
  if (npcDialogue.classList.contains('open')) {
    showToast('先按 E 完成与维修工程师的对话');
    return;
  }
  setInventoryOpen(!state.inventoryOpen);
}

function toggleEra() {
  if (npcDialogue.classList.contains('open')) {
    showToast('先按 E 完成与维修工程师的对话');
    return;
  }
  if (state.bossAwake && !state.boss.defeated) {
    showToast('掘脉者的时间扰动场正在封锁时代切换；强制停机后才能离开');
    return;
  }
  if (state.elevatorRiding) {
    showToast('升降机强电磁场正在干扰时间锚；到站后才能切换时代');
    return;
  }
  if (state.eraTarget < .5 && isLowerLevel() && player.x > bossDoorX + .45) {
    showToast('Boss维护区的时相屏障会撕裂切换坐标；先从内侧返回实验室再切换时代');
    return;
  }
  if (state.eraTarget < .5 && isLowerLevel() && state.maintenanceScanActive) {
    showToast('最终扫描尚未结束；回到控制台完成验收和封门后才能切换时代');
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
    showToast('2047年：掘脉者即将永久封存，03号防爆门正在做最后一次检修');
  } else if (state.eraTarget < .5) {
    showToast('固定时间点：2047年，03号矿场仍在正常运行');
  } else if (inLab && state.history.chargesInstalled) {
    showToast('2147年：你当年暗装的时锁爆破器仍藏在门框内，外侧接收器已经收到信号');
  } else if (inLab) {
    showToast('2147年：Boss防爆门已经锈死成墙；外部记录注明只能从内侧定向爆破');
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
  state.npcDialogueStep = 0;
  state.maintenanceScanActive = false;
  state.lastNpcWarning = -10;
  state.doorBlast = 0;
  state.bossAwake = false;
  state.attackTimer = 0;
  state.attackDuration = 0;
  state.attackCooldown = 0;
  state.weaponReadyTimer = 0;
  state.wrenchInFlight = false;
  state.wrenchReleasePending = false;
  state.attackHeld = false;
  state.equippedWeapon = 'calibrator';
  state.attackWeapon = 'calibrator';
  state.attackAimX = 1;
  state.attackAimY = 0;
  state.meleeHit = false;
  state.boss.health = state.boss.maxHealth;
  state.boss.x = bossHomeX;
  state.boss.direction = -1;
  state.boss.beamPhase = 'cooldown';
  state.boss.beamTimer = 10;
  state.boss.beamOriginX = bossHomeX + 1.12;
  state.boss.beamOriginY = labGroundY + 3.58;
  state.boss.beamDirectionX = -1;
  state.boss.beamDirectionY = 0;
  state.boss.beamHit = false;
  state.boss.laserPhase = 'idle';
  state.boss.laserTimer = 1.85;
  state.boss.attackIndex = 0;
  state.boss.airY = 0;
  state.boss.jumpStartX = bossHomeX;
  state.boss.jumpTargetX = bossHomeX;
  state.boss.drillAngle = Math.PI;
  state.boss.drillLength = 0;
  state.boss.drillHit = false;
  state.boss.dashDirection = -1;
  state.boss.dashHit = false;
  state.boss.dashStartX = bossHomeX;
  state.boss.jumpInvulnerable = false;
  state.boss.hitFlash = 0;
  state.boss.defeated = false;
  state.cameraX = 0;
  state.cameraY = 0;
  state.inventoryOpen = false;
  state.inventory.handwheel = false;
  state.inventory.breachKit = true;
  state.history.wheelCollected = false;
  state.history.wheelCrossed = false;
  state.history.wheelInstalled = false;
  state.history.gateOpened = false;
  state.history.elevatorUsed = false;
  state.history.workOrderFound = false;
  state.history.bossBriefed = false;
  state.history.chargesInstalled = false;
  state.history.scanFinalized = false;
  state.history.doorBreached = false;
  inventoryPanel.classList.remove('open');
  inventoryPanel.setAttribute('aria-hidden', 'true');
  closeNpcDialogue();
  player.crouching = false;
  player.crouchBlend = 0;
  player.halfH = player.standingHalfH;
  player.dropThroughTimer = 0;
  player.x = -10.4;
  player.y = groundY + player.halfH;
  player.vx = 0;
  player.vy = 0;
  player.supportY = groundY;
  player.facing = 1;
  player.aimX = 1;
  player.aimY = 0;
  player.walkPhase = 0;
  player.walkBlend = 0;
  player.health = player.maxHealth;
  player.hurtCooldown = 0;
  pointerAim.initialized = false;
  clearCombatEffects();
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
  player.supportY = state.elevatorY;
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
  const direction = state.inventoryOpen || npcDialogue.classList.contains('open')
    ? 0
    : (keys.has('KeyD') ? 1 : 0) - (keys.has('KeyA') ? 1 : 0);
  const movementSpeed = player.speed * (player.crouching ? .52 : 1);
  player.vx = THREE.MathUtils.damp(player.vx, direction * movementSpeed, direction ? 15 : 22, dt);
  if (direction) player.facing = direction;
  const lowerLevel = isLowerLevel();
  const minX = lowerLevel ? 16.9 : -14.7;
  const maxX = lowerLevel ? bossArenaEndX - .55 : 20.25;
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
    const blockedByPastBossDoor = state.eraTarget < .5
      && !state.maintenanceScanActive
      && overlaps(nextX, player.halfW, bossDoorX, .72);
    const blockedByPastSafetyLine = state.eraTarget < .5
      && overlaps(nextX, player.halfW, 63.1, .2);
    const blockedByModernBossDoor = state.eraTarget > .5
      && state.doorBlast < .72
      && overlaps(nextX, player.halfW, bossDoorX, .82);
    const blockedByBattleGate = state.eraTarget > .5
      && state.bossAwake
      && !state.boss.defeated
      && overlaps(nextX, player.halfW, 62.45, .34);
    if (blockedByPastLabDoor || blockedByPastBossDoor || blockedByPastSafetyLine || blockedByModernBossDoor || blockedByBattleGate) {
      if (blockedByPastSafetyLine && clock.elapsedTime - state.lastNpcWarning > 2.2) {
        state.lastNpcWarning = clock.elapsedTime;
        showToast('林工（广播）：7C-113，维修范围到门框内侧锁轨为止。封存舱已经移交，请退回门边。');
      }
      const obstacleX = blockedByPastLabDoor
        ? labEntranceX
        : (blockedByPastBossDoor || blockedByModernBossDoor
          ? bossDoorX
          : (blockedByPastSafetyLine ? 63.1 : 62.45));
      nextX = player.x < obstacleX
        ? obstacleX - .9 - player.halfW
        : obstacleX + .9 + player.halfW;
      player.vx = 0;
    }
  }
  if (assetEditor) nextX = assetEditor.resolveHorizontalPlayer(player, player.x, nextX);
  player.x = nextX;
}

function setPlayerCrouching(crouching) {
  if (player.crouching === crouching) return;
  const feetY = player.y - player.halfH;
  player.crouching = crouching;
  player.halfH = crouching ? player.crouchHalfH : player.standingHalfH;
  player.y = feetY + player.halfH;
}

function tryDropThroughPlatform() {
  if (!isLowerLevel() || !player.grounded || player.supportY <= labGroundY + .2) return;
  player.dropThroughTimer = .18;
  player.grounded = false;
  player.vy = -2.4;
  player.y -= .12;
}

function updateCrouch(dt) {
  player.dropThroughTimer = Math.max(0, player.dropThroughTimer - dt);
  const wantsCrouch = keys.has('KeyS') || keys.has('ControlLeft') || keys.has('ControlRight');
  setPlayerCrouching(wantsCrouch && !state.elevatorRiding && !state.inventoryOpen);
}

function updateVertical(dt) {
  if (state.elevatorRiding) return;
  const previousBottom = player.y - player.halfH;
  player.vy -= 24 * dt;
  let nextY = player.y + player.vy * dt;
  const lowerLevel = isLowerLevel();
  let landingY = lowerLevel ? labGroundY : groundY;

  const nextBottom = nextY - player.halfH;
  if (lowerLevel && player.vy <= 0 && player.dropThroughTimer <= 0) {
    for (const platform of bossPlatforms) {
      const overPlatform = Math.abs(player.x - platform.x) <= platform.width * .5 + player.halfW * .7;
      const crossedTop = previousBottom >= platform.top - .09 && nextBottom <= platform.top;
      if (overPlatform && crossedTop) landingY = Math.max(landingY, platform.top);
    }
  }
  if (assetEditor && player.vy <= 0) {
    landingY = assetEditor.resolveLandingY(player, previousBottom, nextBottom, landingY);
  }
  if (player.vy <= 0 && nextBottom <= landingY) {
    nextY = landingY + player.halfH;
    player.vy = 0;
    player.grounded = true;
    player.supportY = landingY;
  } else {
    player.grounded = false;
  }
  player.y = nextY;
}

function tryJump() {
  if (state.inventoryOpen || npcDialogue.classList.contains('open') || state.elevatorRiding || !player.grounded || player.crouching) return;
  player.vy = player.jumpSpeed;
  player.grounded = false;
}

function handleInteraction() {
  if (state.inventoryOpen) return;
  if (npcDialogue.classList.contains('open')) {
    advanceNpcDialogue();
    return;
  }
  const lowerLevel = isLowerLevel();
  const nearElevator = Math.abs(player.x - elevatorX) < (lowerLevel ? 2.8 : 1.85);
  const nearLabEntrance = lowerLevel && Math.abs(player.x - labEntranceX) < 1.65;
  const nearWorkOrder = lowerLevel && Math.abs(player.x - workOrderX) < 1.35;
  const nearMaintenanceNpc = lowerLevel && Math.abs(player.x - maintenanceNpcX) < 1.45;
  const nearScanConsole = lowerLevel && Math.abs(player.x - scanConsoleX) < 1.2;
  const nearChargeSocket = lowerLevel && Math.abs(player.x - chargeSocketX) < 1.0;
  const nearModernDetonator = lowerLevel && Math.abs(player.x - modernDetonatorX) < 1.25;
  const playerNearPickup = Math.abs(player.x - handwheelPickupX) < 1.75;
  const playerNearSocket = Math.abs(player.x - winchSocketX) < 2.0;

  if (nearElevator) {
    if (state.eraTarget > .5) {
      if (lowerLevel && state.elevatorAtBottom) {
        // Avoid a soft lock at the lower seam: interacting with the ruined
        // lift automatically recalls its intact 2047 counterpart and rides up.
        state.eraTarget = 0;
        state.pulse = 1;
        state.elevatorRiding = true;
        state.elevatorTargetY = groundY;
        player.x = elevatorX;
        player.vx = 0;
        keys.clear();
        showToast('时间锚已切回2047年：完整升降机正在返回地面层');
        updateHud();
        return;
      }
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

  if (nearWorkOrder) {
    if (state.eraTarget > .5 && !state.history.workOrderFound) {
      state.history.workOrderFound = true;
      state.pulse = 1;
      showToast('残存档案：维修员工号7C-113 · 工单M-03-7721 · 03号防爆门锁轨与密封检修');
      updateHud();
    } else if (state.eraTarget > .5) {
      showToast('档案确认：7C-113当日负责防爆门终检；掘脉者在检修结束后立即永久封存');
    } else if (state.history.workOrderFound) {
      showToast('原始工单仍在桌上；你已经从2147年记住了工号、工单号和维修任务');
    } else {
      showToast('桌上文件属于当日内部资料，外来人员不能直接翻阅；现代残存档案也许保留了内容');
    }
    return;
  }

  if (nearMaintenanceNpc && state.eraTarget < .5 && !state.maintenanceScanActive) {
    if (!state.history.workOrderFound) {
      showToast('维修工程师：报一下员工工号、工单号和今天的检修项目。');
    } else if (state.history.scanFinalized) {
      showToast('维修工程师：门体数据合格，封存胶已经灌注。03号舱不会再开启。');
    } else if (state.history.bossBriefed) {
      showToast('工程师：工号核验完了。到右侧控制台启动门体终检。');
    } else {
      state.npcDialogueStep = 0;
      advanceNpcDialogue();
    }
    return;
  }

  if (nearScanConsole && state.eraTarget < .5) {
    if (!state.history.workOrderFound) {
      showToast('控制台拒绝访问：需要有效的防爆门维护工单');
    } else if (!state.history.bossBriefed) {
      showToast('控制台要求维修工程师确认；先和左侧工作人员交谈');
    } else if (state.maintenanceScanActive && state.history.chargesInstalled && player.x < bossDoorX - .7) {
      state.maintenanceScanActive = false;
      state.history.scanFinalized = true;
      state.pulse = 1;
      showToast('门体开闭、锁止与密封数据全部合格；工程师灌注封存胶，暗槽从此无人复检');
      updateHud();
    } else if (!state.maintenanceScanActive) {
      if (state.history.scanFinalized) {
        showToast('最终维护已经验收并封存，扫描程序不能再次启动');
      } else {
        state.maintenanceScanActive = true;
        state.pulse = 1;
        showToast('防爆门终检启动：工作人员撤入隔离室，监控转为门体数据采集，门保持开启');
        updateHud();
      }
    } else {
      showToast(state.history.chargesInstalled
        ? '从门框内侧返回控制台，再按 E 结束终检并封门'
        : '终检进行中：进入门框内侧完成官方锁轨检修，并寻找暗装位置');
    }
    return;
  }

  if (nearChargeSocket && state.eraTarget < .5) {
    if (!state.maintenanceScanActive) {
      showToast('门框内侧锁轨被关闭的防爆门挡住；先启动门体终检');
    } else if (!state.history.chargesInstalled) {
      if (!state.inventory.breachKit) {
        showToast('时间锚背包中没有时锁爆破器，无法暗装');
        return;
      }
      state.inventory.breachKit = false;
      state.history.chargesInstalled = true;
      state.pulse = 1;
      showToast('趁监控只采集门体数据，你在锁轨背面铣出暗槽，嵌入时锁爆破器，再用同色封板复原');
      updateHud();
    } else {
      showToast('自制暗槽已被同色封板遮住；时锁爆破器休眠，不向2047年的检测设备发出信号');
    }
    return;
  }

  if (nearModernDetonator && state.eraTarget > .5) {
    if (!state.history.chargesInstalled) {
      showToast('门体与墙体已经锈结；爆破工具必须预先藏在门框内侧，外部无法安全施工');
    } else if (!state.history.doorBreached) {
      state.history.doorBreached = true;
      state.pulse = 1.8;
      showToast('2147年时间锚信号确认：暗槽中的爆破器沿预切轨迹从内部切断门框');
      updateHud();
    } else {
      showToast('门框已经沿暗槽切断，等待厚重门体完全倒向封存舱内部');
    }
    return;
  }

  if (lowerLevel) {
    if (state.eraTarget < .5 && Math.abs(player.x - bossDoorX) < 2.0 && !state.maintenanceScanActive) {
      showToast('2047年防爆门受维护权限锁定；先和工程师交谈并启动最终扫描');
    } else if (state.eraTarget < .5 && player.x > bossDoorX + .45) {
      showToast(state.history.chargesInstalled
        ? '暗槽已经伪装完成；返回控制台结束终检，让工作人员按计划永久封门'
        : '这里是门框内侧锁轨。靠近按 E，完成检修时偷偷加工暗槽并藏入爆破器');
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
    && state.history.doorBreached
    && state.doorBlast > .82
    && isLowerLevel()
    && player.x > bossTriggerX
    && !state.bossAwake
  ) {
    state.bossAwake = true;
    state.pulse = 1;
    state.boss.beamPhase = 'cooldown';
    state.boss.beamTimer = 10;
    state.boss.laserPhase = 'idle';
    state.boss.laserTimer = 1.85;
    showToast('掘脉者解除封存：一阶段约每2.5秒释放一次锁定激光；半血后改为短激光弹幕和高速冲刺跳钻');
    updateHud();
  }
}

function tryAttack() {
  if (state.inventoryOpen || npcDialogue.classList.contains('open') || state.elevatorRiding || state.attackCooldown > 0) return;
  if (state.equippedWeapon === 'returnWrench' && state.wrenchInFlight) {
    showToast('回弹扳手还没有回到手中');
    return;
  }
  refreshAimDirection();
  const weapon = weaponDefinitions[state.equippedWeapon];
  state.attackWeapon = state.equippedWeapon;
  state.attackAimX = player.aimX;
  state.attackAimY = player.aimY;
  state.meleeHit = false;
  state.attackDuration = weapon.duration;
  state.attackTimer = state.attackDuration;
  state.attackCooldown = weapon.cooldown;
  state.weaponReadyTimer = 1.2;
  if (state.attackWeapon === 'impactHammer') {
    // One click commits to one fixed forward arc; the weapon remains locked to the striking arm.
    state.attackAimX = player.facing;
    state.attackAimY = 0;
  }
  if (state.attackWeapon === 'coreDrill' || state.attackWeapon === 'impactHammer') return;
  if (state.attackWeapon === 'returnWrench') {
    state.wrenchReleasePending = true;
    return;
  }
  const mesh = new THREE.Group();
  rectangle(mesh, .3, .075, '#bffaff', 0, 0, .06, .98);
  ring(mesh, .105, .06, '#66d8e4', -.11, 0, .08, .82, 16);
  mesh.rotation.z = Math.atan2(player.aimY, player.aimX);
  mesh.position.set(
    player.x + player.aimX * 1.06,
    player.y + .12 + player.aimY * 1.06,
    PLAYER_PROJECTILE_Z,
  );
  enforceProjectileForeground(mesh);
  playerShotLayer.add(mesh);
  playerShots.push({
    type: 'pulse',
    mesh,
    vx: player.aimX * 18.5,
    vy: player.aimY * 18.5,
    damage: 24,
    life: 2.6,
    age: 0,
  });
}

function releaseWrench() {
  if (!state.wrenchReleasePending || state.wrenchInFlight) return;
  const mesh = createReturnWrench();
  mesh.scale.setScalar(.82);
  mesh.rotation.z = Math.atan2(state.attackAimY, state.attackAimX);
  mesh.position.set(
    player.x + state.attackAimX * .88,
    player.y + .12 + state.attackAimY * .88,
    PLAYER_PROJECTILE_Z,
  );
  enforceProjectileForeground(mesh);
  playerShotLayer.add(mesh);
  playerShots.push({
    type: 'wrench',
    mesh,
    vx: state.attackAimX * 11.5,
    vy: state.attackAimY * 11.5,
    damage: 20,
    life: 3.6,
    age: 0,
    returning: false,
    hitCore: false,
  });
  state.wrenchReleasePending = false;
  state.wrenchInFlight = true;
}

function updateWrenchThrow() {
  if (!state.wrenchReleasePending || state.attackWeapon !== 'returnWrench') return;
  const progress = 1 - state.attackTimer / Math.max(.001, state.attackDuration);
  if (progress >= .46) releaseWrench();
}

function clearCombatEffects() {
  for (const shot of playerShots) playerShotLayer.remove(shot.mesh);
  playerShots.length = 0;
  for (const wave of bossShockwaves) bossAttackLayer.remove(wave.mesh);
  bossShockwaves.length = 0;
  for (const shot of bossLaserShots) bossAttackLayer.remove(shot.mesh);
  bossLaserShots.length = 0;
  state.wrenchInFlight = false;
  state.wrenchReleasePending = false;
  state.boss.laserPhase = 'idle';
  state.boss.laserTimer = 1.85;
  beamTelegraphMesh.visible = false;
  beamGlowMesh.visible = false;
  beamCoreMesh.visible = false;
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
    player.crouching = false;
    player.crouchBlend = 0;
    player.halfH = player.standingHalfH;
    player.dropThroughTimer = 0;
    player.x = 64.2;
    player.y = labGroundY + player.halfH;
    player.vx = 0;
    player.vy = 0;
    player.supportY = labGroundY;
    state.boss.health = state.boss.maxHealth;
    state.boss.x = bossHomeX;
    state.boss.direction = -1;
    state.boss.beamPhase = 'cooldown';
    state.boss.beamTimer = 10;
    state.boss.beamHit = false;
    state.boss.laserPhase = 'idle';
    state.boss.laserTimer = 1.85;
    state.boss.attackIndex = 0;
    state.boss.airY = 0;
    state.boss.drillLength = 0;
    state.boss.drillHit = false;
    state.boss.dashDirection = -1;
    state.boss.dashHit = false;
    state.boss.jumpInvulnerable = false;
    clearCombatEffects();
    showToast('时间锚将你重构在Boss房入口；掘脉者的攻击序列已经恢复');
  } else {
    showToast(`被掘脉者攻击命中：-${amount} · 剩余生命 ${Math.ceil(player.health)}`);
  }
  updateHud();
  return defeated;
}

function damageBoss(amount) {
  if (!state.bossAwake || state.boss.defeated || state.eraTarget < .5) return;
  if (state.boss.jumpInvulnerable) {
    state.boss.hitFlash = .05;
    return;
  }
  state.boss.health = Math.max(0, state.boss.health - amount);
  state.boss.hitFlash = .16;
  if (state.boss.health <= 0) {
    state.boss.defeated = true;
    state.boss.airY = 0;
    state.boss.drillLength = 0;
    state.boss.beamPhase = 'cooldown';
    state.boss.laserPhase = 'idle';
    state.boss.laserTimer = 1.85;
    state.boss.jumpInvulnerable = false;
    state.pulse = 1;
    clearCombatEffects();
    showToast('掘脉者被强制停机：Boss房封锁解除，时相核心可以安全回收');
  }
  updateHud();
}

function updateMeleeAttack() {
  const weapon = weaponDefinitions[state.attackWeapon];
  if (state.attackTimer <= 0 || !weapon?.melee || state.meleeHit) return;
  const progress = 1 - state.attackTimer / Math.max(.001, state.attackDuration);
  if (progress < weapon.activeStart || progress > weapon.activeEnd) return;

  const originX = player.x;
  const originY = player.y + .12;
  const impactStrength = state.attackWeapon === 'impactHammer' ? 13 : 8;
  const fractured = assetEditor?.fracturePhysicsAlongSegment(
    originX,
    originY,
    originX + state.attackAimX * weapon.range,
    originY + state.attackAimY * weapon.range,
    state.attackWeapon === 'impactHammer' ? .48 : .26,
    state.attackAimX * impactStrength,
    state.attackAimY * impactStrength + 1.5,
  );
  if (fractured) {
    state.meleeHit = true;
    state.pulse = Math.max(state.pulse, state.attackWeapon === 'impactHammer' ? .42 : .24);
    return;
  }

  if (!state.bossAwake || state.boss.defeated || state.eraTarget < .5 || !isLowerLevel()) return;
  const dx = state.boss.x + .18 - originX;
  const dy = bossCoreY + state.boss.airY - originY;
  const distance = Math.max(.001, Math.hypot(dx, dy));
  const aimDot = (dx * state.attackAimX + dy * state.attackAimY) / distance;
  if (distance > weapon.range || aimDot < weapon.minDot) return;

  state.meleeHit = true;
  state.pulse = Math.max(state.pulse, state.attackWeapon === 'impactHammer' ? .42 : .24);
  damageBoss(weapon.damage);
}

function bossIsJumping() {
  return state.boss.beamPhase === 'jumpRise'
    || state.boss.beamPhase === 'jumpHover'
    || state.boss.beamPhase === 'jumpFall'
    || state.boss.beamPhase === 'jumpImpact';
}

function beginBossDash() {
  clearBossLaserShots();
  state.boss.laserPhase = 'idle';
  state.boss.laserTimer = .45;
  beamTelegraphMesh.visible = false;
  beamGlowMesh.visible = false;
  beamCoreMesh.visible = false;
  state.boss.beamPhase = 'dashCharge';
  state.boss.beamTimer = 1.25;
  state.boss.dashDirection = Math.sign(player.x - state.boss.x) || state.boss.direction;
  state.boss.direction = state.boss.dashDirection;
  state.boss.dashStartX = state.boss.x;
  state.boss.dashHit = false;
}

function startBossDashActive(dt) {
  state.boss.beamPhase = 'dashActive';
  state.boss.beamTimer = .9;
  state.boss.dashHit = false;
  state.pulse = Math.max(state.pulse, .32);
  // The exhaust telegraph ends on this frame and the machine immediately gains forward motion.
  const speed = state.boss.health <= state.boss.maxHealth * .5 ? 32 : 26;
  state.boss.x += state.boss.dashDirection * speed * Math.min(dt, 1 / 60);
}

function beginBossSlam() {
  state.boss.beamPhase = 'jumpRise';
  state.boss.beamTimer = .35;
  state.boss.jumpStartX = state.boss.x;
  state.boss.jumpTargetX = THREE.MathUtils.clamp(player.x, state.boss.patrolMinX, state.boss.patrolMaxX);
  state.boss.airY = 0;
  state.boss.drillLength = 0;
  state.boss.drillHit = false;
  state.boss.jumpInvulnerable = true;
}

function finishBossAttack() {
  const phaseTwo = state.boss.health <= state.boss.maxHealth * .5;
  state.boss.beamPhase = 'cooldown';
  state.boss.beamTimer = phaseTwo ? 5.6 : 10;
  state.boss.laserPhase = 'idle';
  state.boss.laserTimer = phaseTwo ? .5 : 1.85;
  state.boss.airY = 0;
  state.boss.drillLength = 0;
  state.boss.jumpInvulnerable = false;
  beamTelegraphMesh.visible = false;
  beamGlowMesh.visible = false;
  beamCoreMesh.visible = false;
}

function beginBossLaser() {
  state.boss.laserPhase = 'telegraph';
  state.boss.laserTimer = .45;
  state.boss.beamHit = false;
  state.boss.direction = Math.sign(player.x - state.boss.x) || state.boss.direction;
  state.boss.beamOriginX = state.boss.x + (state.boss.direction < 0 ? .72 : -.72);
  state.boss.beamOriginY = labGroundY + 3.58 + state.boss.airY;
  const targetX = player.x;
  const targetY = player.y + .06;
  const length = Math.max(.001, Math.hypot(targetX - state.boss.beamOriginX, targetY - state.boss.beamOriginY));
  state.boss.beamDirectionX = (targetX - state.boss.beamOriginX) / length;
  state.boss.beamDirectionY = (targetY - state.boss.beamOriginY) / length;
}

function spawnBossLaserShot() {
  state.boss.direction = Math.sign(player.x - state.boss.x) || state.boss.direction;
  const originX = state.boss.x + (state.boss.direction < 0 ? .72 : -.72);
  const originY = labGroundY + 3.58;
  const targetAngle = Math.atan2(player.y + .06 - originY, player.x - originX);
  const spreadPattern = [0, -.07, .07, -.13, .13];
  const angle = targetAngle + spreadPattern[state.boss.attackIndex % spreadPattern.length];
  state.boss.attackIndex += 1;
  state.boss.beamDirectionX = Math.cos(angle);
  state.boss.beamDirectionY = Math.sin(angle);
  state.boss.direction = state.boss.beamDirectionX < 0 ? -1 : 1;

  const mesh = new THREE.Group();
  rectangle(mesh, 1.25, .2, '#58d5e3', 0, 0, .04, .48);
  disc(mesh, .1, '#58d5e3', -.625, 0, .04, .48, 18);
  disc(mesh, .1, '#58d5e3', .625, 0, .04, .48, 18);
  rectangle(mesh, .98, .07, '#efffff', 0, 0, .08, .98);
  disc(mesh, .036, '#efffff', -.49, 0, .08, .98, 16);
  disc(mesh, .036, '#efffff', .49, 0, .08, .98, 16);
  mesh.position.set(originX, originY, 3.08);
  mesh.rotation.z = angle;
  bossAttackLayer.add(mesh);
  bossLaserShots.push({
    mesh,
    vx: Math.cos(angle) * 10.5,
    vy: Math.sin(angle) * 10.5,
    life: 4.2,
    hit: false,
  });
  state.pulse = Math.max(state.pulse, .16);
}

function clearBossLaserShots() {
  for (const shot of bossLaserShots) bossAttackLayer.remove(shot.mesh);
  bossLaserShots.length = 0;
}

function updateBossLaserShots(dt) {
  for (let index = bossLaserShots.length - 1; index >= 0; index--) {
    const shot = bossLaserShots[index];
    shot.life -= dt;
    shot.mesh.position.x += shot.vx * dt;
    shot.mesh.position.y += shot.vy * dt;
    shot.mesh.scale.y = .92 + Math.sin(shot.life * 28) * .08;
    if (!shot.hit && Math.hypot(shot.mesh.position.x - player.x, shot.mesh.position.y - player.y) < .62) {
      shot.hit = true;
      bossAttackLayer.remove(shot.mesh);
      bossLaserShots.splice(index, 1);
      if (damagePlayer(10, Math.sign(shot.vx) || 1)) return true;
      continue;
    }
    const outsideArena = shot.mesh.position.x < 62
      || shot.mesh.position.x > bossArenaEndX + 1
      || shot.mesh.position.y < labGroundY - 2
      || shot.mesh.position.y > labGroundY + 10.5;
    if (shot.life <= 0 || outsideArena) {
      bossAttackLayer.remove(shot.mesh);
      bossLaserShots.splice(index, 1);
    }
  }
  return false;
}

function updateBossLaser(dt) {
  const phaseTwo = state.boss.health <= state.boss.maxHealth * .5;
  if (phaseTwo) {
    beamTelegraphMesh.visible = false;
    beamGlowMesh.visible = false;
    beamCoreMesh.visible = false;
    if (state.boss.beamPhase !== 'cooldown') {
      state.boss.laserPhase = 'idle';
      return false;
    }
    state.boss.laserTimer -= dt;
    if (state.boss.laserPhase === 'bullet') {
      if (state.boss.laserTimer <= 0) {
        state.boss.laserPhase = 'idle';
        state.boss.laserTimer = .34;
      }
    } else if (state.boss.laserTimer <= 0) {
      spawnBossLaserShot();
      state.boss.laserPhase = 'bullet';
      state.boss.laserTimer = .08;
    }
    return false;
  }

  if (state.boss.beamPhase !== 'cooldown') {
    state.boss.laserPhase = 'idle';
    beamTelegraphMesh.visible = false;
    beamGlowMesh.visible = false;
    beamCoreMesh.visible = false;
    return false;
  }

  if (state.boss.laserPhase === 'idle') {
    state.boss.laserTimer -= dt;
    if (state.boss.laserTimer <= 0) beginBossLaser();
    else return false;
  }

  state.boss.beamOriginX = state.boss.x + (state.boss.direction < 0 ? .72 : -.72);
  state.boss.beamOriginY = labGroundY + 3.58;
  state.boss.laserTimer -= dt;

  if (state.boss.laserPhase === 'telegraph') {
    const warningPulse = .045 + Math.abs(Math.sin(state.boss.laserTimer * 22)) * .055;
    setBeamMesh(beamTelegraphMesh, warningPulse, true);
    beamGlowMesh.visible = false;
    beamCoreMesh.visible = false;
    if (state.boss.laserTimer <= 0) {
      state.boss.laserPhase = 'active';
      state.boss.laserTimer = .2;
      state.pulse = Math.max(state.pulse, .42);
    }
  } else if (state.boss.laserPhase === 'active') {
    beamTelegraphMesh.visible = false;
    setBeamMesh(beamGlowMesh, .34, true);
    setBeamMesh(beamCoreMesh, .085, true);
    if (!state.boss.beamHit && playerDistanceToBeam() < .55) {
      state.boss.beamHit = true;
      if (damagePlayer(20, Math.sign(state.boss.beamDirectionX) || 1)) return true;
    }
    if (state.boss.laserTimer <= 0) {
      state.boss.laserPhase = 'idle';
      state.boss.laserTimer = 1.85;
      beamGlowMesh.visible = false;
      beamCoreMesh.visible = false;
    }
  }
  return false;
}

function playerTouchesChargingSaw() {
  const sawX = state.boss.x + (state.boss.dashDirection < 0 ? -1.82 : 1.82);
  const sawY = labGroundY + 2.37;
  const closestX = THREE.MathUtils.clamp(sawX, player.x - player.halfW, player.x + player.halfW);
  const closestY = THREE.MathUtils.clamp(sawY, player.y - player.halfH, player.y + player.halfH);
  return Math.hypot(sawX - closestX, sawY - closestY) < .96;
}

function spawnBossShockwave(direction) {
  const mesh = new THREE.Group();
  for (let index = 0; index < 3; index++) {
    polygon(mesh, [[0, 0], [.28, .34 + index * .08], [.56, 0]], index === 1 ? '#d8ffff' : '#69d7e3', index * .3, 0, .04 + index * .01, .82 - index * .14);
  }
  mesh.position.set(state.boss.x + direction * 1.0, labGroundY + .05, 3.0);
  mesh.scale.x = direction;
  bossAttackLayer.add(mesh);
  bossShockwaves.push({ mesh, direction, speed: 8.4, life: 1.5, hit: false });
}

function updateBossShockwaves(dt) {
  for (let index = bossShockwaves.length - 1; index >= 0; index--) {
    const wave = bossShockwaves[index];
    wave.life -= dt;
    wave.mesh.position.x += wave.direction * wave.speed * dt;
    wave.mesh.scale.y = .9 + Math.sin((1.5 - wave.life) * 18) * .12;
    if (!wave.hit && Math.abs(wave.mesh.position.x - player.x) < .55 && player.y < labGroundY + 2.15) {
      wave.hit = true;
      if (damagePlayer(16, wave.direction)) return true;
    }
    if (wave.life <= 0 || wave.mesh.position.x < 63 || wave.mesh.position.x > bossArenaEndX) {
      bossAttackLayer.remove(wave.mesh);
      bossShockwaves.splice(index, 1);
    }
  }
  return false;
}

function setBeamMesh(mesh, thickness, visible) {
  const x1 = state.boss.beamOriginX;
  const y1 = state.boss.beamOriginY;
  const x2 = x1 + state.boss.beamDirectionX * state.boss.beamLength;
  const y2 = y1 + state.boss.beamDirectionY * state.boss.beamLength;
  mesh.position.set((x1 + x2) * .5, (y1 + y2) * .5, 3.05);
  mesh.rotation.z = Math.atan2(y2 - y1, x2 - x1);
  mesh.scale.set(state.boss.beamLength, thickness, 1);
  mesh.visible = visible;
}

function playerDistanceToBeam() {
  const px = player.x - state.boss.beamOriginX;
  const py = player.y - state.boss.beamOriginY;
  const along = px * state.boss.beamDirectionX + py * state.boss.beamDirectionY;
  if (along < 0 || along > state.boss.beamLength) return Infinity;
  return Math.abs(px * state.boss.beamDirectionY - py * state.boss.beamDirectionX);
}

function updateCombat(dt, elapsed) {
  const wasAttacking = state.attackTimer > 0;
  state.attackCooldown = Math.max(0, state.attackCooldown - dt);
  state.attackTimer = Math.max(0, state.attackTimer - dt);
  state.weaponReadyTimer = wasAttacking
    ? 1.2
    : Math.max(0, state.weaponReadyTimer - dt);
  updateWrenchThrow();
  if (state.attackHeld && state.equippedWeapon === 'coreDrill' && state.attackCooldown <= 0) tryAttack();
  player.hurtCooldown = Math.max(0, player.hurtCooldown - dt);
  state.boss.hitFlash = Math.max(0, state.boss.hitFlash - dt);
  updateMeleeAttack();

  for (let index = playerShots.length - 1; index >= 0; index--) {
    const shot = playerShots[index];
    shot.life -= dt;
    shot.age += dt;
    if (shot.type === 'wrench') {
      shot.mesh.rotation.z -= dt * 13;
      if (!shot.returning) {
        const canTrackBoss = state.bossAwake
          && !state.boss.defeated
          && state.eraTarget > .5
          && isLowerLevel();
        if (canTrackBoss) {
          const targetX = state.boss.x + .18;
          const targetY = bossCoreY + state.boss.airY;
          const currentAngle = Math.atan2(shot.vy, shot.vx);
          const desiredAngle = Math.atan2(targetY - shot.mesh.position.y, targetX - shot.mesh.position.x);
          const angleDifference = Math.atan2(
            Math.sin(desiredAngle - currentAngle),
            Math.cos(desiredAngle - currentAngle),
          );
          const nextAngle = currentAngle + THREE.MathUtils.clamp(angleDifference, -6.2 * dt, 6.2 * dt);
          const outboundSpeed = Math.max(11.5, Math.hypot(shot.vx, shot.vy));
          shot.vx = Math.cos(nextAngle) * outboundSpeed;
          shot.vy = Math.sin(nextAngle) * outboundSpeed;
        }
        if (shot.age > 1.3) shot.returning = true;
      }
      if (shot.returning) {
        const dx = player.x - shot.mesh.position.x;
        const dy = player.y + .12 - shot.mesh.position.y;
        const distance = Math.max(.001, Math.hypot(dx, dy));
        shot.vx = dx / distance * 13.5;
        shot.vy = dy / distance * 13.5;
        if (distance < .48 && shot.age > .45) {
          playerShotLayer.remove(shot.mesh);
          playerShots.splice(index, 1);
          state.wrenchInFlight = false;
          continue;
        }
      }
    }
    const previousShotX = shot.mesh.position.x;
    const previousShotY = shot.mesh.position.y;
    shot.mesh.position.x += shot.vx * dt;
    shot.mesh.position.y += shot.vy * dt;
    const hitPhysicsAsset = assetEditor?.fracturePhysicsAlongSegment(
      previousShotX,
      previousShotY,
      shot.mesh.position.x,
      shot.mesh.position.y,
      shot.type === 'wrench' ? .3 : .14,
      shot.vx,
      shot.vy,
    );
    if (hitPhysicsAsset) {
      state.pulse = Math.max(state.pulse, shot.type === 'wrench' ? .3 : .2);
      if (shot.type === 'wrench') {
        shot.returning = true;
        continue;
      }
      playerShotLayer.remove(shot.mesh);
      playerShots.splice(index, 1);
      continue;
    }
    const hitCore = state.bossAwake
      && !state.boss.defeated
      && state.eraTarget > .5
      && Math.abs(shot.mesh.position.x - (state.boss.x + .18)) < .92
      && Math.abs(shot.mesh.position.y - (bossCoreY + state.boss.airY)) < 1.0;
    if (hitCore) {
      if (shot.type === 'wrench') {
        if (!shot.hitCore) {
          shot.hitCore = true;
          shot.returning = true;
          damageBoss(shot.damage);
        }
      } else {
        playerShotLayer.remove(shot.mesh);
        playerShots.splice(index, 1);
        damageBoss(shot.damage);
      }
    } else {
      const shotInLab = shot.mesh.position.y < -12;
      const outsideHorizontalBounds = shotInLab
        ? shot.mesh.position.x < 15.5 || shot.mesh.position.x > bossArenaEndX + 1
        : shot.mesh.position.x < -18 || shot.mesh.position.x > 23;
      const outsideVerticalBounds = shotInLab
        ? shot.mesh.position.y < labGroundY - 2 || shot.mesh.position.y > labGroundY + 10.5
        : shot.mesh.position.y < groundY - 2 || shot.mesh.position.y > groundY + 11;
      if (shot.life <= 0 || outsideHorizontalBounds || outsideVerticalBounds) {
        playerShotLayer.remove(shot.mesh);
        playerShots.splice(index, 1);
        if (shot.type === 'wrench') state.wrenchInFlight = false;
      }
    }
  }

  const fighting = state.bossAwake && !state.boss.defeated && state.eraTarget > .5 && isLowerLevel();
  if (!fighting) {
    state.boss.laserPhase = 'idle';
    clearBossLaserShots();
    beamTelegraphMesh.visible = false;
    beamGlowMesh.visible = false;
    beamCoreMesh.visible = false;
    return;
  }

  if (updateBossLaserShots(dt)) return;
  if (updateBossLaser(dt)) return;

  // Contact is harmless during patrol. Phase one uses a 10s saw cadence; phase two shortens the cooldown to 5.6s.
  if (state.boss.beamPhase === 'cooldown' && state.boss.laserPhase === 'idle') {
    const patrolSpeed = state.boss.health <= state.boss.maxHealth * .5 ? 3.8 : 3.0;
    state.boss.x += state.boss.direction * patrolSpeed * dt;
    if (state.boss.x <= state.boss.patrolMinX) {
      state.boss.x = state.boss.patrolMinX;
      state.boss.direction = 1;
    } else if (state.boss.x >= state.boss.patrolMaxX) {
      state.boss.x = state.boss.patrolMaxX;
      state.boss.direction = -1;
    }
  }

  const phaseTwo = state.boss.health <= state.boss.maxHealth * .5;
  if (phaseTwo && state.boss.beamPhase === 'cooldown') {
    state.boss.beamTimer = Math.min(state.boss.beamTimer, 5.6);
  }
  state.boss.beamTimer -= dt;
  if (state.boss.beamPhase === 'cooldown' && state.boss.beamTimer <= 0) {
    beginBossDash();
  } else if (state.boss.beamPhase === 'dashCharge') {
    if (state.boss.beamTimer <= 0) {
      startBossDashActive(dt);
    }
  } else if (state.boss.beamPhase === 'dashActive') {
    const dashSpeed = state.boss.health <= state.boss.maxHealth * .5 ? 32 : 26;
    state.boss.x += state.boss.dashDirection * dashSpeed * dt;
    const reachedWall = state.boss.x <= state.boss.patrolMinX || state.boss.x >= state.boss.patrolMaxX;
    state.boss.x = THREE.MathUtils.clamp(state.boss.x, state.boss.patrolMinX, state.boss.patrolMaxX);
    if (!state.boss.dashHit && playerTouchesChargingSaw()) {
      state.boss.dashHit = true;
      const defeated = damagePlayer(26, state.boss.dashDirection);
      if (defeated) return;
    }
    if (state.boss.beamTimer <= 0 || reachedWall) {
      state.boss.beamPhase = 'dashRecover';
      state.boss.beamTimer = .55;
    }
  } else if (state.boss.beamPhase === 'dashRecover') {
    if (state.boss.beamTimer <= 0) {
      if (state.boss.health <= state.boss.maxHealth * .5) beginBossSlam();
      else finishBossAttack();
    }
  } else if (state.boss.beamPhase === 'jumpRise') {
    const progress = THREE.MathUtils.clamp(1 - state.boss.beamTimer / .35, 0, 1);
    state.boss.jumpTargetX = THREE.MathUtils.damp(state.boss.jumpTargetX, player.x, 3.2, dt);
    state.boss.x = THREE.MathUtils.lerp(state.boss.jumpStartX, state.boss.jumpTargetX, THREE.MathUtils.smoothstep(progress, 0, 1));
    state.boss.airY = THREE.MathUtils.lerp(0, 5.8, THREE.MathUtils.smoothstep(progress, 0, 1));
    if (state.boss.beamTimer <= 0) {
      state.boss.beamPhase = 'jumpHover';
      state.boss.beamTimer = .2;
    }
  } else if (state.boss.beamPhase === 'jumpHover') {
    state.boss.airY = 5.8 + Math.sin(elapsed * 8) * .08;
    state.boss.x = THREE.MathUtils.damp(state.boss.x, player.x, 4.2, dt);
    if (state.boss.beamTimer <= 0) {
      state.boss.beamPhase = 'jumpFall';
      state.boss.beamTimer = .24;
      state.boss.jumpStartX = state.boss.x;
      state.boss.jumpTargetX = THREE.MathUtils.clamp(player.x, state.boss.patrolMinX, state.boss.patrolMaxX);
      state.boss.drillLength = 0;
    }
  } else if (state.boss.beamPhase === 'jumpFall') {
    const progress = THREE.MathUtils.clamp(1 - state.boss.beamTimer / .24, 0, 1);
    state.boss.airY = THREE.MathUtils.lerp(5.8, 0, Math.pow(progress, .68));
    state.boss.x = THREE.MathUtils.lerp(state.boss.jumpStartX, state.boss.jumpTargetX, THREE.MathUtils.smoothstep(progress, 0, 1));
    state.boss.drillLength = THREE.MathUtils.lerp(0, 2.35, THREE.MathUtils.smoothstep(progress, .08, .72));
    if (state.boss.beamTimer <= 0) {
      state.boss.airY = 0;
      state.boss.beamPhase = 'jumpImpact';
      state.boss.beamTimer = .5;
      state.boss.jumpInvulnerable = false;
      state.pulse = 1;
      if (!state.boss.drillHit && Math.abs(player.x - state.boss.x) < 1.25 && player.y < labGroundY + 2.0) {
        state.boss.drillHit = true;
        const defeated = damagePlayer(34, Math.sign(player.x - state.boss.x) || 1);
        if (defeated) return;
      }
    }
  } else if (state.boss.beamPhase === 'jumpImpact') {
    state.boss.drillLength = 2.35 * THREE.MathUtils.clamp(state.boss.beamTimer / .5, 0, 1);
    if (state.boss.beamTimer <= 0) finishBossAttack();
  }
}

function updateHud() {
  eventCrate.classList.toggle('active', state.history.wheelCollected);
  eventPlate.classList.toggle('active', state.history.wheelCrossed);
  eventGate.classList.toggle('active', state.history.gateOpened);
  eventElevator.classList.toggle('active', state.history.elevatorUsed);
  eventWorkOrder.classList.toggle('active', state.history.workOrderFound);
  eventReactor.classList.toggle('active', state.history.chargesInstalled);
  eventFuture.classList.toggle('active', state.history.doorBreached);
  eventCrate.querySelector('span').textContent = state.history.wheelCollected ? '手轮已从闸门拆下' : '尚未发生';
  eventPlate.querySelector('span').textContent = state.history.wheelCrossed ? '时间锚携带成功' : '等待背包';
  eventGate.querySelector('span').textContent = state.history.gateOpened ? '机械卡扣保持开启' : '等待物品';
  eventElevator.querySelector('span').textContent = state.history.elevatorUsed ? '已抵达地下实验室' : '等待进入';
  eventWorkOrder.querySelector('span').textContent = state.history.workOrderFound ? '7C-113 / M-03-7721' : '等待调查';
  eventReactor.querySelector('span').textContent = state.history.chargesInstalled ? '暗槽与爆破器已伪装' : '尚未发生';
  eventFuture.querySelector('span').textContent = state.history.doorBreached ? '门框已沿暗槽切断' : '等待改写';

  const lowerLevel = isLowerLevel();
  const beforeLabEntrance = lowerLevel && player.x < labEntranceX - .75;
  if (state.boss.defeated) {
    objective.textContent = 'Boss已击败：掘脉者被强制停机，时相核心现在可以安全取出';
  } else if (state.bossAwake) {
    objective.textContent = state.boss.jumpInvulnerable
      ? '二阶段：Boss腾空后快速坠落 · 约0.8秒钻头落地 · 立即离开锁定位置'
      : state.boss.health <= state.boss.maxHealth * .5
        ? '二阶段：约5.6秒短激光弹幕 · 清场喷气后锯齿冲刺 · 冲刺后接快速跳钻'
        : 'Boss战：测绘激光约每2.5秒释放一次 · 激光贯穿速度不变 · S/Ctrl下蹲躲避高速冲刺';
  } else if (state.elevatorRiding) {
    objective.textContent = state.elevatorTargetY === labGroundY
      ? '2047年：升降机正在下降到地下实验室，时间切换暂时受到干扰'
      : '2047年：升降机正在返回地面层，时间切换暂时受到干扰';
  } else if (beforeLabEntrance && state.eraTarget < .5) {
    objective.textContent = '2047年井底：实验室安全门权限锁死。按 Q 去2147年穿过坍塌后的同一入口';
  } else if (beforeLabEntrance) {
    objective.textContent = '2147年井底：实验室安全门已经坍塌，向右穿过缺口进入实验室';
  } else if (lowerLevel && state.eraTarget > .5 && !state.history.workOrderFound) {
    objective.textContent = '2147年实验室：前往旧档案台，按 E 查找当年维修员的工号和防爆门维修任务';
  } else if (lowerLevel && state.eraTarget < .5 && !state.history.workOrderFound) {
    objective.textContent = '2047年：没有员工身份和工单内容无法取得维修权限。按 Q 回2147年调查旧档案';
  } else if (lowerLevel && state.eraTarget < .5 && !state.history.bossBriefed) {
    objective.textContent = '2047年封存日：向右找到维修工程师，按 E 报出工号7C-113、工单M-03-7721和维修任务';
  } else if (lowerLevel && state.eraTarget < .5 && state.history.scanFinalized) {
    objective.textContent = '2047年：工程师已经验收并永久封门。按 Q 回2147年激活你藏在门框暗槽里的爆破器';
  } else if (lowerLevel && state.eraTarget < .5 && !state.maintenanceScanActive) {
    objective.textContent = '2047年：身份核验通过。到右侧控制台按 E，让人员撤入隔离室并开启防爆门终检';
  } else if (lowerLevel && state.eraTarget < .5 && !state.history.chargesInstalled) {
    objective.textContent = player.x > bossDoorX + .4
      ? '2047年门框内侧：靠近锁轨按 E，在完成检修时偷偷铣出暗槽并藏入时锁爆破器'
      : '2047年终检中：防爆门已经开启，进入内侧锁轨维修位，但不要越过封存舱警戒线';
  } else if (lowerLevel && state.eraTarget < .5) {
    objective.textContent = player.x > bossDoorX + .4
      ? '2047年：暗槽伪装完成，先返回实验室一侧；封存舱警戒区内无法切换时代'
      : '2047年：暗槽伪装完成。回到控制台按 E 结束终检，让工程师验收并永久封门';
  } else if (lowerLevel && !state.history.chargesInstalled) {
    objective.textContent = '2147年：防爆门锈死成墙，外侧无法施工。按 Q 回2047年，利用合法维修机会接近门框内侧';
  } else if (lowerLevel && !state.history.doorBreached) {
    objective.textContent = '2147年：门框暗槽里的时锁爆破器已响应。靠近墙左侧接收器按 E 激活';
  } else if (lowerLevel && state.doorBlast < .82) {
    objective.textContent = '2147年：门框正在沿预切暗槽断裂，等待厚重门体完全倒向封存舱内部';
  } else if (lowerLevel) {
    objective.textContent = '2147年：Boss通道已打开。越过倒塌门体，进入房间取出掘脉者的时相核心';
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
    doorStatus.querySelector('span').textContent = '03 CONTAINMENT BAY';
    if (state.eraTarget < .5) {
      doorPower.textContent = state.history.scanFinalized
        ? 'M-03-7721：验收通过 · 封存完成'
        : (state.maintenanceScanActive ? '最终扫描：进行中 · 人员已隔离' : '最终维护：进行中 · 人员在场');
      doorLock.textContent = state.history.scanFinalized
        ? '防爆门：最终关闭 · 内侧面板已封存'
        : (state.history.chargesInstalled
          ? '锁轨与密封：数据合格 · 暗槽未被检测'
          : (state.maintenanceScanActive ? '防爆门：维护开启 · 内侧锁轨可检修' : '防爆门：权限锁定'));
    } else {
      doorPower.textContent = state.history.chargesInstalled ? '时间锚链路：门框暗槽信号在线' : '防爆门：断电 · 门框完全锈死';
      doorLock.textContent = state.history.doorBreached
        ? '门框：已定向切断 · 门体向内倒塌'
        : (state.history.chargesInstalled ? '外侧接收器：等待引爆指令' : '安全记录：仅允许从内侧破门');
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
  const nearElevator = Math.abs(player.x - elevatorX) < (lowerLevel ? 2.8 : 1.85);
  const nearLabEntrance = lowerLevel && Math.abs(player.x - labEntranceX) < 1.65;
  const nearWorkOrder = lowerLevel && Math.abs(player.x - workOrderX) < 1.35;
  const nearMaintenanceNpc = lowerLevel && Math.abs(player.x - maintenanceNpcX) < 1.45;
  const nearScanConsole = lowerLevel && Math.abs(player.x - scanConsoleX) < 1.2;
  const nearChargeSocket = lowerLevel && Math.abs(player.x - chargeSocketX) < 1.0;
  const nearModernDetonator = lowerLevel && Math.abs(player.x - modernDetonatorX) < 1.25;
  const nearBossDoor = lowerLevel && Math.abs(player.x - bossDoorX) < 1.6;
  const nearPickup = Math.abs(player.x - handwheelPickupX) < 1.75;
  const nearSocket = Math.abs(player.x - winchSocketX) < 2.0;
  let message = '';

  if (state.inventoryOpen || npcDialogue.classList.contains('open')) {
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
  } else if (nearWorkOrder && state.eraTarget > .5) {
    message = state.history.workOrderFound
      ? '旧档案：工号7C-113 · M-03-7721 · 防爆门锁轨与密封检修'
      : '按 E 读取2147年残存档案，寻找维修员工号和防爆门任务';
  } else if (nearWorkOrder) {
    message = state.history.workOrderFound
      ? '2047年原始工单 · 你已从现代档案记住工号、编号和维修任务'
      : '内部文件不可直接翻阅 · 前往2147年寻找废弃档案';
  } else if (nearMaintenanceNpc && state.eraTarget < .5 && !state.maintenanceScanActive) {
    message = !state.history.workOrderFound
      ? '工程师要求提供员工工号、工单号和检修项目'
      : (state.history.scanFinalized
        ? '按 E 查看工程师的最终封存验收结果'
        : (state.history.bossBriefed
          ? '工号核验通过 · 到右侧控制台启动防爆门终检'
          : '按 E 报出7C-113、M-03-7721和“锁轨与密封检修”'));
  } else if (nearScanConsole && state.eraTarget < .5) {
    message = !state.history.workOrderFound
      ? '控制台拒绝访问 · 缺少防爆门维护工单'
      : (!state.history.bossBriefed
        ? '最终扫描需要工程师确认 · 先与左侧工作人员交谈'
        : (state.history.scanFinalized
          ? 'M-03-7721验收完成 · 防爆门已最终封存'
          : (state.maintenanceScanActive
            ? (state.history.chargesInstalled
              ? '按 E 结束扫描 · 关闭防爆门并让工程师完成验收'
              : '最终扫描进行中 · 人员和监控已进入隔离状态')
            : '按 E 启动防爆门终检 · 人员撤离并打开门体维护模式')));
  } else if (nearChargeSocket && state.eraTarget < .5) {
    message = state.history.chargesInstalled
      ? '锁轨封板已经复原 · 时锁爆破器处于无信号休眠状态'
      : (state.maintenanceScanActive
        ? '按 E 在检修锁轨时偷偷铣出暗槽，并藏入时锁爆破器'
        : '内侧锁轨被防爆门挡住 · 先启动门体终检');
  } else if (nearModernDetonator && state.eraTarget > .5) {
    message = !state.history.chargesInstalled
      ? '外侧无法安全施工 · 必须在2047年利用维修身份接近门框内侧'
      : (state.history.doorBreached
        ? '门框已经沿暗槽切断 · 厚重门体正在倒向封存舱内部'
        : '按 E 向你在2047年暗装的时锁爆破器发送激活信号');
  } else if (nearBossDoor && state.eraTarget < .5) {
    message = state.history.scanFinalized
      ? '最终维护验收完成 · 防爆门已经永久封闭'
      : (state.maintenanceScanActive
        ? '门框内侧锁轨已经开放 · 黄色警戒线之后是即将封存的03号机舱'
        : '2047年防爆门权限锁定 · 凭员工工号和工单取得门体维护授权');
  } else if (nearBossDoor) {
    message = state.doorBlast >= .82
      ? '门体已经向房间内部倒塌 · Boss通道开放'
      : '2147年门扇与墙体锈结 · 安全记录：只能从内侧定向爆破';
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
  if (pastMaintenanceNpc) {
    const npcTargetX = state.maintenanceScanActive ? 48.3 : maintenanceNpcX;
    pastMaintenanceNpc.position.x = THREE.MathUtils.damp(pastMaintenanceNpc.position.x, npcTargetX, 3.8, dt);
  }
  if (pastScanDoor) {
    const shutterTargetY = state.maintenanceScanActive ? -2.65 : 0;
    pastScanDoor.position.y = THREE.MathUtils.damp(pastScanDoor.position.y, shutterTargetY, 5, dt);
  }
  if (pastScanCore) {
    pastScanCore.rotation.z += dt * (state.maintenanceScanActive ? 2.1 : .25);
    const scanScale = state.maintenanceScanActive ? 1 + Math.sin(elapsed * 5.4) * .04 : 1;
    pastScanCore.scale.setScalar(scanScale);
  }
  if (pastBossDoor) {
    const doorTargetY = state.maintenanceScanActive ? 6.55 : 0;
    pastBossDoor.position.y = THREE.MathUtils.damp(pastBossDoor.position.y, doorTargetY, 5.5, dt);
  }
  if (pastChargeSockets) {
    const socketVisibility = state.maintenanceScanActive ? 1 : 0;
    setLayerOpacity(pastChargeSockets, pastAmount * socketVisibility);
    if (pastChargeSockets.userData.concealedTool) {
      pastChargeSockets.userData.concealedTool.material.opacity = state.history.chargesInstalled
        ? .78 * pastAmount * socketVisibility
        : 0;
    }
  }
  if (modernWorkOrder) setLayerOpacity(modernWorkOrder, state.era * (state.history.workOrderFound ? .58 : 1));
  state.doorBlast = THREE.MathUtils.damp(state.doorBlast, state.history.doorBreached ? 1 : 0, 2.8, dt);
  const blastEase = state.doorBlast * state.doorBlast * (3 - state.doorBlast * 2);
  if (modernBossDoor) {
    modernBossDoor.rotation.z = -1.43 * blastEase;
  }
  if (modernDoorRubble) setLayerOpacity(modernDoorRubble, state.era * blastEase);
  if (modernBlastFx) {
    const flash = state.history.doorBreached ? Math.max(0, Math.sin(Math.min(1, state.doorBlast * 2.15) * Math.PI)) : 0;
    setLayerOpacity(modernBlastFx, state.era * flash);
    modernBlastFx.scale.setScalar(.65 + state.doorBlast * 1.35);
  }
  if (bossBattleBarrier) {
    setLayerOpacity(bossBattleBarrier, state.era * (state.bossAwake && !state.boss.defeated ? 1 : 0));
  }
  if (pastBoss) {
    pastBoss.rotation.z = 0;
    if (pastBoss.userData.cutter) pastBoss.userData.cutter.rotation.z = elapsed * .32;
    if (pastBoss.userData.legs) {
      for (const leg of pastBoss.userData.legs) leg.rotation.z = Math.sin(elapsed * .65 + leg.userData.phase) * .015;
    }
  }
  if (modernBoss) {
    modernBoss.position.x = state.boss.x;
    const bossWalking = state.bossAwake
      && !state.boss.defeated
      && state.boss.beamPhase === 'cooldown'
      && state.boss.laserPhase === 'idle';
    const gaitBob = bossWalking ? Math.abs(Math.sin(elapsed * 7.2)) * .075 : 0;
    const bossTargetY = state.boss.defeated ? labGroundY - .08 : labGroundY + .04 + gaitBob + state.boss.airY;
    const bossVerticalDamp = state.boss.beamPhase === 'jumpFall'
      ? 30
      : state.boss.beamPhase.startsWith('jump') ? 18 : 6;
    modernBoss.position.y = THREE.MathUtils.damp(modernBoss.position.y, bossTargetY, bossVerticalDamp, dt);
    const jumpTilt = state.boss.beamPhase === 'jumpRise' ? -.08 : state.boss.beamPhase === 'jumpFall' ? .09 : 0;
    const bossTargetRotation = state.boss.defeated ? -.08 : jumpTilt;
    modernBoss.rotation.z = THREE.MathUtils.damp(modernBoss.rotation.z, bossTargetRotation, 5, dt);
    const bossScale = state.boss.defeated ? .96 : 1;
    const facingScale = state.boss.direction < 0 ? 1 : -1;
    modernBoss.scale.x = THREE.MathUtils.damp(modernBoss.scale.x, bossScale * facingScale, 8, dt);
    modernBoss.scale.y = THREE.MathUtils.damp(modernBoss.scale.y, bossScale, 5, dt);
    modernBoss.scale.z = bossScale;
    if (modernBoss.userData.cutter) {
      const sawSpeed = state.boss.beamPhase === 'dashCharge'
        ? 13
        : state.boss.beamPhase === 'dashActive' ? 22 : (bossWalking ? 1.5 : .25);
      modernBoss.userData.cutter.rotation.z += dt * sawSpeed;
    }
    if (modernBoss.userData.legs) {
      for (const leg of modernBoss.userData.legs) {
        const slamPose = state.boss.beamPhase === 'jumpRise' || state.boss.beamPhase === 'jumpFall'
          ? Math.sin(leg.userData.phase) * .2
          : 0;
        const stride = bossWalking ? Math.sin(elapsed * 7.2 + leg.userData.phase) * .13 : slamPose;
        leg.rotation.z = THREE.MathUtils.damp(leg.rotation.z, stride, 10, dt);
      }
    }
    if (modernBoss.userData.bellyDrill) {
      const bellyDrill = modernBoss.userData.bellyDrill;
      const drillAttack = state.boss.beamPhase === 'jumpFall' || state.boss.beamPhase === 'jumpImpact';
      bellyDrill.visible = drillAttack && state.boss.drillLength > .08;
      bellyDrill.rotation.z = -Math.PI * .5;
      const drillExtension = THREE.MathUtils.clamp(state.boss.drillLength / 2.35, 0, 1);
      bellyDrill.scale.set(1, 1, 1);
      bellyDrill.userData.bit.position.x = .08;
      bellyDrill.userData.bit.rotation.z = 0;
      bellyDrill.userData.bit.scale.x = .38 + drillExtension * .72;
      bellyDrill.userData.bit.scale.y = drillAttack ? 1.16 + Math.sin(elapsed * 36) * .045 : 1.16;
      const hatchOpen = state.boss.beamPhase === 'jumpHover' || drillAttack;
      bellyDrill.userData.hatch.scale.setScalar(1 + (hatchOpen ? .16 + Math.sin(elapsed * 10) * .05 : 0));
    }
    if (modernBoss.userData.dashJets) {
      const jetting = state.boss.beamPhase === 'dashCharge';
      modernBoss.userData.dashJets.visible = jetting;
      if (jetting) modernBoss.userData.dashJets.scale.x = .75 + Math.abs(Math.sin(elapsed * 17)) * .7;
    }
    if (modernBoss.userData.emitter) {
      const laserAiming = state.boss.laserPhase === 'telegraph'
        || state.boss.laserPhase === 'active'
        || state.boss.laserPhase === 'bullet';
      const beamAngle = Math.atan2(state.boss.beamDirectionY, state.boss.beamDirectionX);
      const emitterAngle = laserAiming
        ? (state.boss.direction < 0 ? beamAngle : Math.PI - beamAngle)
        : Math.sin(elapsed * .7) * .35 + Math.PI;
      modernBoss.userData.emitter.rotation.z = THREE.MathUtils.damp(modernBoss.userData.emitter.rotation.z, emitterAngle, 9, dt);
    }
  }
  for (let index = 0; index < bossCoreMaterials.length; index++) {
    const eraAmount = index === 0 ? 1 - state.era : state.era;
    bossCoreMaterials[index].opacity = (.62 + Math.sin(elapsed * (state.bossAwake ? 5.2 : 2.0) + index) * .24) * eraAmount;
  }
  if (bossCoreMaterials[1]) bossCoreMaterials[1].color.set(
    state.boss.jumpInvulnerable ? '#ffd26f' : (state.boss.hitFlash > 0 ? '#ffffff' : '#82edf6'),
  );

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

  refreshAimDirection();
  const rig = playerMesh.userData.rig;
  player.crouchBlend = THREE.MathUtils.damp(player.crouchBlend, player.crouching ? 1 : 0, 16, dt);
  const crouch = player.crouchBlend;
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
  const attacking = state.attackTimer > 0;
  const attackProgress = attacking && state.attackDuration > 0
    ? 1 - state.attackTimer / state.attackDuration
    : 0;
  const airborne = player.grounded ? 0 : 1;
  let leftLegTarget = airborne ? -.23 : gaitPose.leftHip * player.walkBlend;
  let rightLegTarget = airborne ? .3 : gaitPose.rightHip * player.walkBlend;
  let leftKneeTarget = airborne ? -.32 : gaitPose.leftKnee * player.walkBlend;
  let rightKneeTarget = airborne ? -.12 : gaitPose.rightKnee * player.walkBlend;
  if (crouch > .001 && !airborne) {
    leftLegTarget = THREE.MathUtils.lerp(leftLegTarget, .52, crouch);
    rightLegTarget = THREE.MathUtils.lerp(rightLegTarget, -.44, crouch);
    leftKneeTarget = THREE.MathUtils.lerp(leftKneeTarget, -1.08, crouch);
    rightKneeTarget = THREE.MathUtils.lerp(rightKneeTarget, .94, crouch);
  }
  let leftArmTarget = airborne ? .22 : armStride;
  let rightArmTarget = airborne ? -.22 : -armStride;
  const localAimAngle = Math.atan2(player.aimY, Math.abs(player.aimX));
  const attackAimAngle = Math.atan2(state.attackAimY, Math.abs(state.attackAimX));
  const posedWeapon = attacking ? state.attackWeapon : state.equippedWeapon;
  const weaponAvailableInHand = posedWeapon !== 'returnWrench' || !state.wrenchInFlight;
  const weaponDrawn = attacking
    || (state.weaponReadyTimer > 0 && weaponAvailableInHand && posedWeapon !== 'impactHammer');
  let weaponAimAngle = localAimAngle;
  let recoil = 0;
  if (attacking && posedWeapon === 'calibrator') {
    recoil = Math.sin(attackProgress * Math.PI) * .07;
  } else if (attacking && posedWeapon === 'coreDrill') {
    weaponAimAngle = attackAimAngle + Math.sin(attackProgress * Math.PI * 4) * .045;
  } else if (attacking && posedWeapon === 'impactHammer') {
    // Terraria-style use animation: one arm and the hammer are a rigid unit rotating once around the shoulder.
    const swingT = THREE.MathUtils.smoothstep(attackProgress, 0, .78);
    weaponAimAngle = THREE.MathUtils.lerp(1.08, -.9, swingT);
  } else if (attacking && posedWeapon === 'returnWrench') {
    const throwT = THREE.MathUtils.smoothstep(attackProgress, .05, .7);
    weaponAimAngle = attackAimAngle + THREE.MathUtils.lerp(.82, -.36, throwT);
  }
  if (weaponDrawn) rightArmTarget = weaponAimAngle + Math.PI * .5 - recoil;
  if (attacking && weaponDrawn) {
    leftArmTarget = posedWeapon === 'coreDrill'
      ? rightArmTarget - .34
      : .12;
  }
  rig.leftLeg.rotation.z = THREE.MathUtils.damp(rig.leftLeg.rotation.z, leftLegTarget, 15, dt);
  rig.rightLeg.rotation.z = THREE.MathUtils.damp(rig.rightLeg.rotation.z, rightLegTarget, 15, dt);
  rig.leftKnee.rotation.z = THREE.MathUtils.damp(rig.leftKnee.rotation.z, leftKneeTarget, 17, dt);
  rig.rightKnee.rotation.z = THREE.MathUtils.damp(rig.rightKnee.rotation.z, rightKneeTarget, 17, dt);
  const armResponse = attacking && posedWeapon === 'impactHammer'
    ? 52
    : attacking && posedWeapon === 'coreDrill' ? 30 : 13;
  rig.leftArm.rotation.z = THREE.MathUtils.damp(rig.leftArm.rotation.z, leftArmTarget, armResponse, dt);
  rig.rightArm.rotation.z = THREE.MathUtils.damp(rig.rightArm.rotation.z, rightArmTarget, armResponse, dt);

  const stepBounce = Math.abs(Math.sin(player.walkPhase * 2)) * .045 * player.walkBlend;
  rig.bodyRig.position.y = rig.bodyBaseY + stepBounce - crouch * .18;
  rig.headRig.position.y = rig.headBaseY + stepBounce * .72 - crouch * .38;
  rig.leftLeg.position.y = THREE.MathUtils.damp(rig.leftLeg.position.y, rig.leftLegBaseY + crouch * .34, 18, dt);
  rig.rightLeg.position.y = THREE.MathUtils.damp(rig.rightLeg.position.y, rig.rightLegBaseY + crouch * .34, 18, dt);
  const hammerLean = 0;
  rig.bodyRig.rotation.z = THREE.MathUtils.damp(rig.bodyRig.rotation.z, hammerLean, 18, dt);
  rig.headRig.rotation.z = THREE.MathUtils.damp(rig.headRig.rotation.z, hammerLean * .35, 18, dt);
  rig.leftArm.position.y = rig.leftArmBaseY + stepBounce - crouch * .25;
  rig.rightArm.position.y = rig.rightArmBaseY + stepBounce - crouch * .25;
  const jumpHeight = Math.max(0, player.y - (player.supportY + player.halfH));
  rig.shadow.position.y = THREE.MathUtils.lerp(rig.shadowBaseY, -.5, crouch) - jumpHeight;
  rig.shadow.scale.x = 1 - Math.min(.42, jumpHeight * .12) + player.walkBlend * .06;
  rig.shadow.scale.y = .24 - Math.min(.08, jumpHeight * .018) + Math.sin(player.walkPhase * 2) * .015 * player.walkBlend;
  rig.shadow.material.opacity = .24 - Math.min(.13, jumpHeight * .035);

  weaponRig.visible = weaponDrawn;
  calibratorMesh.visible = weaponDrawn && posedWeapon === 'calibrator';
  coreDrillMesh.visible = weaponDrawn && posedWeapon === 'coreDrill';
  impactHammerMesh.visible = weaponDrawn && posedWeapon === 'impactHammer';
  returnWrenchMesh.visible = weaponDrawn && !state.wrenchInFlight && posedWeapon === 'returnWrench';
  holsteredCalibrator.visible = !weaponDrawn && state.equippedWeapon === 'calibrator';
  stowedDrill.visible = !weaponDrawn && state.equippedWeapon === 'coreDrill';
  stowedHammer.visible = !weaponDrawn && state.equippedWeapon === 'impactHammer';
  holsteredWrench.visible = !weaponDrawn && !state.wrenchInFlight && state.equippedWeapon === 'returnWrench';
  calibratorMesh.rotation.z = 0;
  if (coreDrillMesh.userData.bit) {
    const drillBit = coreDrillMesh.userData.bit;
    drillBit.rotation.z = 0;
    drillBit.position.y = attacking && posedWeapon === 'coreDrill' ? Math.sin(elapsed * 42) * .018 : 0;
    for (let index = 0; index < drillBit.userData.flutes.length; index++) {
      drillBit.userData.flutes[index].material.opacity = attacking && posedWeapon === 'coreDrill'
        ? .3 + ((elapsed * 10 + index * .34) % 1) * .7
        : .58;
    }
  }
  calibratorMesh.position.x = attacking && posedWeapon === 'calibrator' ? -Math.sin(attackProgress * Math.PI) * .055 : 0;
  weaponRig.rotation.z = -Math.PI * .5;
  muzzleFlash.visible = weaponDrawn && attacking && posedWeapon === 'calibrator' && attackProgress < .52;
  muzzleFlash.scale.setScalar(.62 + Math.sin(attackProgress * Math.PI) * .28);

  if (player.hurtCooldown > .55) {
    for (const item of playerMesh.userData.bodyMaterials) item.color.set('#ff9b8d');
  }

  playerMesh.position.set(player.x, player.y, PLAYER_FOREGROUND_Z);
  playerMesh.scale.x = player.facing;
  playerMesh.scale.y = THREE.MathUtils.damp(playerMesh.scale.y, 1, 18, dt);
  playerMesh.rotation.z = THREE.MathUtils.damp(playerMesh.rotation.z, -player.vx * .008, 12, dt);

  const viewHalfWidth = 9 * (innerWidth / innerHeight);
  let cameraTargetX;
  let cameraTargetY;
  if (state.elevatorRiding) {
    cameraTargetX = elevatorX;
    cameraTargetY = player.y + 3.0;
  } else if (isLowerLevel()) {
    cameraTargetX = THREE.MathUtils.clamp(player.x, 17 + viewHalfWidth, bossArenaEndX - viewHalfWidth);
    cameraTargetY = labGroundY + 4.15;
  } else {
    cameraTargetX = THREE.MathUtils.clamp(player.x, -17 + viewHalfWidth, 22 - viewHalfWidth);
    cameraTargetY = 0;
  }
  if (artTourEnabled) {
    const tourCamera = sampleArtTour(elapsed);
    cameraTargetX = tourCamera.x;
    cameraTargetY = tourCamera.y;
  } else if (artTourStop) {
    cameraTargetX = artTourStop.x;
    cameraTargetY = artTourStop.y;
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
  if (assetEditor?.handleGameKeyDown(event)) return;
  if (!event.repeat && event.code === 'Digit1') equipWeapon('calibrator');
  if (!event.repeat && event.code === 'Digit2') equipWeapon('coreDrill');
  if (!event.repeat && event.code === 'Digit3') equipWeapon('impactHammer');
  if (!event.repeat && event.code === 'Digit4') equipWeapon('returnWrench');
  if (!event.repeat && event.code === 'KeyJ') {
    state.attackHeld = state.equippedWeapon === 'coreDrill';
    tryAttack();
  }
  if (!event.repeat && event.code === 'KeyB') toggleInventory();
  if (!event.repeat && event.code === 'Escape' && state.inventoryOpen) setInventoryOpen(false);
  if (!event.repeat && event.code === 'KeyQ') toggleEra();
  if (!event.repeat && event.code === 'KeyR') resetHistory();
  if (!event.repeat && event.code === 'KeyE') handleInteraction();
  if (!event.repeat && (event.code === 'KeyW' || event.code === 'Space')) tryJump();
  if (!event.repeat && (event.code === 'KeyS' || event.code === 'ControlLeft' || event.code === 'ControlRight')) tryDropThroughPlatform();
  keys.add(event.code);
  if (['Space', 'KeyW', 'KeyA', 'KeyB', 'KeyD', 'KeyE', 'KeyJ', 'KeyQ', 'KeyS', 'ControlLeft', 'ControlRight', 'Digit1', 'Digit2', 'Digit3', 'Digit4'].includes(event.code)) event.preventDefault();
});
addEventListener('pointermove', event => {
  if (!assetEditor?.isActive()) updatePointerAim(event);
});
addEventListener('pointerdown', event => {
  if (!assetEditor?.isActive() && event.button === 0 && event.target === canvas) {
    state.attackHeld = state.equippedWeapon === 'coreDrill';
    updatePointerAim(event);
    tryAttack();
  }
});
addEventListener('pointerup', event => {
  if (event.button === 0) state.attackHeld = false;
});
addEventListener('pointercancel', () => { state.attackHeld = false; });
addEventListener('keyup', event => {
  keys.delete(event.code);
  if (event.code === 'KeyJ') state.attackHeld = false;
});
addEventListener('blur', () => {
  keys.clear();
  state.attackHeld = false;
});
addEventListener('resize', resize);

// Development preview: opens directly inside the completed Boss room without changing normal saves or progression.
const previewParams = runtimeParams;
if (previewParams.has('boss-preview')) {
  document.body.classList.add('boss-preview-mode');
  state.era = 1;
  state.eraTarget = 1;
  state.gateLift = 1;
  state.doorBlast = 1;
  state.exitReached = true;
  state.elevatorAtBottom = true;
  state.bossAwake = true;
  state.boss.beamPhase = 'cooldown';
  state.boss.beamTimer = 9.7;
  state.boss.laserPhase = 'idle';
  if (previewParams.has('phase2-preview')) state.boss.health = state.boss.maxHealth * .48;
  state.boss.laserTimer = previewParams.has('phase2-preview') ? .35 : 1.85;
  if (previewParams.has('dash-preview')) {
    state.boss.beamPhase = 'dashCharge';
    state.boss.beamTimer = 1.2;
    state.boss.dashDirection = -1;
    state.boss.direction = -1;
  }
  if (previewParams.has('jump-preview')) {
    state.boss.health = state.boss.maxHealth * .48;
    state.boss.beamPhase = 'jumpFall';
    state.boss.beamTimer = .09;
    state.boss.jumpInvulnerable = true;
    state.boss.jumpStartX = 84;
    state.boss.jumpTargetX = 78.5;
    state.boss.x = 84;
    state.boss.airY = 1.15;
    state.boss.drillLength = 2.15;
  }
  Object.assign(state.history, {
    wheelCollected: true,
    wheelCrossed: true,
    wheelInstalled: true,
    gateOpened: true,
    elevatorUsed: true,
    workOrderFound: true,
    bossBriefed: true,
    chargesInstalled: true,
    scanFinalized: true,
    doorBreached: true,
  });
  state.inventory.handwheel = false;
  state.inventory.breachKit = false;
  player.x = 78.5;
  player.y = labGroundY + player.halfH;
  player.vx = 0;
  player.vy = 0;
  player.grounded = true;
  player.supportY = labGroundY;
  if (previewParams.has('laser-preview')) beginBossLaser();
  state.cameraX = 78.5;
  state.cameraY = labGroundY + 4.15;
}

const artPreviewStops = {
  gate: { x: 1, y: 0, playerX: 8.6, lower: false },
  elevator: { x: 18.25, y: -9.2, playerX: 18.25, lower: false },
  entrance: { x: 24, y: labGroundY + 4.15, playerX: 22.8, lower: true },
  laboratory: { x: 46, y: labGroundY + 4.15, playerX: 46, lower: true },
  blastdoor: { x: 60, y: labGroundY + 4.15, playerX: 58.4, lower: true },
  boss: { x: 89, y: labGroundY + 4.15, playerX: 78.5, lower: true },
};
const requestedArtStop = previewParams.get('art-stop');
artTourEnabled = previewParams.has('art-tour');
artTourStop = requestedArtStop ? artPreviewStops[requestedArtStop] || null : null;
if (artTourEnabled || artTourStop) {
  document.body.classList.add('art-preview-mode');
  const previewEra = previewParams.get('era') === 'past' ? 0 : 1;
  state.era = previewEra;
  state.eraTarget = previewEra;
  state.gateLift = 1;
  state.doorBlast = 1;
  state.exitReached = true;
  state.elevatorAtBottom = true;
  Object.assign(state.history, {
    wheelCollected: true,
    wheelCrossed: true,
    wheelInstalled: true,
    gateOpened: true,
    elevatorUsed: true,
    workOrderFound: true,
    bossBriefed: true,
    chargesInstalled: true,
    scanFinalized: true,
    doorBreached: true,
  });
  if (artTourStop) {
    player.x = artTourStop.playerX;
    player.y = (artTourStop.lower ? labGroundY : groundY) + player.halfH;
    player.supportY = artTourStop.lower ? labGroundY : groundY;
    player.grounded = true;
    state.cameraX = artTourStop.x;
    state.cameraY = artTourStop.y;
  }
}

resize();
updateHud();

assetEditor = createAssetEditor({
  scene,
  camera,
  renderer,
  canvas,
  pastLayer,
  presentLayer,
  getEra: () => state.era,
  getGroundForY: y => y < -12 ? labGroundY : groundY,
});
if (previewParams.has('editor-preview')) assetEditor.setActive(true);

let lastRenderedFrame = 0;
function animate(frameTime = 0) {
  requestAnimationFrame(animate);
  if (lowPowerMode && frameTime - lastRenderedFrame < 1000 / 24) return;
  lastRenderedFrame = frameTime;
  const dt = Math.min(clock.getDelta(), .04);
  const elapsed = clock.elapsedTime;
  if (!assetEditor?.isActive()) {
    assetEditor?.updateGamePhysics(dt);
    updateElevator(dt);
    updateCrouch(dt);
    updateHorizontal(dt);
    updateVertical(dt);
    checkHistoryEvents();
    updateCombat(dt, elapsed);
    updateVisuals(dt, elapsed);
    updateHistoryOutcome(dt, elapsed);
    updateInteractionHint();
  }
  assetEditor?.update(dt);
  mineParallaxArt.update(camera.position.x, camera.position.y, state.era);
  enforcePlayerForeground();
  renderer.render(scene, camera);
}

globalThis.__ZERO_ECHO_ART__ = {
  spec: ART_DIRECTION_SPEC,
  tourStops: artPreviewStops,
  getCamera: () => ({ x: camera.position.x, y: camera.position.y }),
};

animate();
