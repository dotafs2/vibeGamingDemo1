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
const inventoryPanel = document.querySelector('#inventory-panel');
const handwheelSlot = document.querySelector('#handwheel-slot');
const inventoryItemName = document.querySelector('#inventory-item-name');
const inventoryItemStatus = document.querySelector('#inventory-item-status');
const doorStatus = document.querySelector('#door-status');
const doorPower = document.querySelector('#door-power');
const doorLock = document.querySelector('#door-lock');

const scene = new THREE.Scene();
const camera = new THREE.OrthographicCamera(-16, 16, 9, -9, 0.1, 100);
camera.position.z = 20;

const renderer = new THREE.WebGLRenderer({ canvas, antialias: true, alpha: false });
renderer.setPixelRatio(Math.min(devicePixelRatio, 2));
renderer.outputColorSpace = THREE.SRGBColorSpace;

const clock = new THREE.Clock();
const keys = new Set();
const groundY = -4.55;

const state = {
  era: 1,
  eraTarget: 1,
  pulse: 0,
  lastToggle: -10,
  gateLift: 0,
  exitReached: false,
  inventoryOpen: false,
  inventory: {
    handwheel: false,
  },
  history: {
    wheelCollected: false,
    wheelCrossed: false,
    wheelInstalled: false,
    gateOpened: false,
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

const background = new THREE.Mesh(new THREE.PlaneGeometry(42, 24), backgroundMaterial);
background.position.z = -12;
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

function buildMineLandmarks(group, palette, ruined) {
  // 相同的三个永久地标：左侧加工厂、中央维修站、右侧闸门机房。
  rectangle(group, 7.5, 4.55, palette.hall, -9.0, -1.62, -2, ruined ? .72 : .92);
  rectangle(group, 5.55, 3.72, palette.workshop, -2.15, -2.04, -2, ruined ? .7 : .9);
  rectangle(group, 3.55, 6.35, palette.gatehouse, 8.35, -1.38, -2.1, ruined ? .68 : .9);
  rectangle(group, 1.18, 7.25, palette.tower, 12.75, -.92, -2.1, ruined ? .75 : .92);
  rectangle(group, 2.25, .38, palette.trim, 12.75, 2.55, -1.8, ruined ? .4 : .78);

  // 同一座圆形储能罐。
  disc(group, 2.25, palette.tank, -11.05, 3.45, -3, ruined ? .13 : .28);
  disc(group, 1.55, palette.trim, -11.05, 3.45, -2.9, ruined ? .055 : .12);

  const leftSupports = [-12.1, -10.75, -9.4, -8.05, -6.7];
  for (let index = 0; index < leftSupports.length; index++) {
    const x = leftSupports[index];
    if (ruined && index === 2) {
      segment(group, x, -3.9, x + .35, -1.25, palette.structure, .23);
    } else {
      segment(group, x, -3.88, x, ruined && index === 4 ? -.95 : .47, palette.structure, ruined ? .24 : .62);
    }
  }
  segment(group, -12.35, .56, -5.65, .56, palette.trim, ruined ? .22 : .65);
  segment(group, -12.0, -3.45, -5.95, .32, palette.structure, ruined ? .14 : .28);
  segment(group, -11.85, .32, -5.85, -3.45, palette.structure, ruined ? .12 : .28);

  const workshopSupports = [-4.45, -3.25, -2.05, -.85, .35];
  for (let index = 0; index < workshopSupports.length; index++) {
    const x = workshopSupports[index];
    if (ruined && index === 3) segment(group, x, -3.82, x + .25, -1.45, palette.structure, .2);
    else segment(group, x, -3.84, x, -.38, palette.structure, ruined ? .2 : .5);
  }
  segment(group, -4.72, -.2, .58, -.2, palette.trim, ruined ? .18 : .55);

  // 同一条从加工厂通向闸门的高架供电管线。
  if (ruined) {
    segment(group, -12.35, 1.25, -6.1, 1.25, palette.conduit, .2);
    segment(group, -5.65, 1.08, -1.25, .72, palette.conduit, .17);
    segment(group, -.7, .78, 2.45, .92, palette.conduit, .17);
    segment(group, 3.05, .82, 5.25, 1.2, palette.conduit, .16);
  } else {
    segment(group, -12.35, 1.25, 5.25, 1.25, palette.conduit, .76);
    segment(group, 5.25, 1.25, 7.35, 3.55, palette.conduit, .62);
  }

  const lampXs = [-10.6, -6.5, -2.35, 1.75, 5.15];
  for (const x of lampXs) {
    const lamp = disc(group, .13, ruined ? palette.deadLamp : palette.lamp, x, .9, -.9, ruined ? .28 : .95);
    if (!ruined) pastLampMaterials.push(lamp.material);
  }

  if (!ruined) {
    for (let index = 0; index < 12; index++) {
      const node = disc(group, .055, palette.lamp, -10.8 + index * 1.38, 1.25, -.75, .5);
      pastPowerNodeMaterials.push(node.material);
    }
    // 正常运转的输送带与维修台。
    rectangle(group, 6.25, .28, palette.trim, -9.0, -3.42, -1.2, .56);
    for (let x = -11.75; x <= -6.25; x += .55) disc(group, .1, palette.lamp, x, -3.42, -1.0, .35);
    rectangle(group, 2.9, .24, palette.trim, -2.15, -3.5, -1.2, .52);
  } else {
    // 原位置的破损输送带、坠落管线和瓦砾。
    const fallenBelt = rectangle(group, 3.1, .3, palette.trim, -10.15, -3.46, -1.2, .28);
    fallenBelt.rotation.z = -.08;
    const fallenPipe = rectangle(group, 2.6, .18, palette.conduit, 1.35, -3.25, -1.1, .22);
    fallenPipe.rotation.z = .18;
    for (let index = 0; index < 7; index++) {
      const chunk = rectangle(group, .32 + (index % 3) * .18, .28 + (index % 2) * .18, palette.debris, -11.5 + index * 1.9, -4.0, -1, .58);
      chunk.rotation.z = -.4 + index * .13;
    }
    segment(group, -11.55, 3.95, -10.3, 2.4, palette.crack, .22);
    segment(group, -10.3, 2.4, -11.0, 1.7, palette.crack, .18);
    segment(group, -3.4, -.35, -2.35, -1.5, palette.crack, .2);
    segment(group, -2.35, -1.5, -2.8, -2.55, palette.crack, .17);
  }
}

function buildPastScene() {
  buildMineLandmarks(pastLayer, {
    hall: '#35191a', workshop: '#43201e', gatehouse: '#321819', tower: '#3d1b1a',
    structure: '#a25735', trim: '#d17645', tank: '#6f1f1d', conduit: '#f08b4b',
    lamp: '#ffb15d', deadLamp: '#7d4d37', debris: '#5e2d22', crack: '#bd6845',
  }, false);
}

function buildPresentScene() {
  buildMineLandmarks(presentLayer, {
    hall: '#132d34', workshop: '#142e34', gatehouse: '#10282f', tower: '#112c33',
    structure: '#4c7b82', trim: '#5f9da6', tank: '#397a83', conduit: '#62adba',
    lamp: '#82d9e5', deadLamp: '#54747a', debris: '#28454c', crack: '#75bec8',
  }, true);
}

buildPastScene();
buildPresentScene();

const commonLayer = new THREE.Group();
scene.add(commonLayer);

const groundMaterial = material('#274048', 1);
const ground = new THREE.Mesh(new THREE.PlaneGeometry(34, .56), groundMaterial);
ground.position.set(0, groundY - .28, .2);
commonLayer.add(ground);

for (let x = -15.5; x <= 15.5; x += 1.05) {
  segment(commonLayer, x, groundY + .01, x + .72, groundY + .01, '#789099', .18, .3);
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

function createPlayer() {
  const group = new THREE.Group();
  const bodyMat = material('#dffaff', .96);
  const trimMat = material('#75cbd6', .86);
  const lineMat = lineMaterial('#e9fdff', .95);
  const shadowMat = material('#000000', .24);

  const shadow = new THREE.Mesh(new THREE.CircleGeometry(.58, 24), shadowMat);
  shadow.scale.y = .24;
  shadow.position.set(0, -.83, -.1);
  group.add(shadow);
  const body = new THREE.Mesh(new THREE.PlaneGeometry(.58, .78), bodyMat);
  body.position.y = -.12;
  group.add(body);
  const head = new THREE.Mesh(new THREE.CircleGeometry(.3, 28), bodyMat);
  head.position.y = .55;
  group.add(head);
  rectangle(group, .19, .16, '#0e272d', .12, .58, .08, .95);
  rectangle(group, .72, .13, '#75cbd6', 0, .18, .05, .78).material = trimMat;
  segment(group, -.19, -.47, -.32, -.83, '#e9fdff', .95, .08).material = lineMat;
  segment(group, .19, -.47, .32, -.83, '#e9fdff', .95, .08).material = lineMat;
  segment(group, -.29, .08, -.53, -.25, '#e9fdff', .9, .08).material = lineMat;
  segment(group, .29, .08, .53, -.25, '#e9fdff', .9, .08).material = lineMat;

  group.userData.bodyMaterials = [bodyMat, trimMat, lineMat];
  return group;
}

const playerMesh = createPlayer();
scene.add(playerMesh);

const particleCount = 150;
const particlePositions = new Float32Array(particleCount * 3);
for (let index = 0; index < particleCount; index++) {
  particlePositions[index * 3] = THREE.MathUtils.randFloatSpread(33);
  particlePositions[index * 3 + 1] = THREE.MathUtils.randFloat(-4.1, 6.8);
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
  if (hasWheel) {
    inventoryItemName.textContent = '03号机械手轮';
    inventoryItemStatus.textContent = '时间锚物品 · 取得于2047年 · 可带往2147年';
  } else if (state.history.wheelInstalled) {
    inventoryItemName.textContent = '手轮已取出';
    inventoryItemStatus.textContent = '当前安装在2147年的03号闸门上';
  } else {
    inventoryItemName.textContent = '空物品栏';
    inventoryItemStatus.textContent = '尚未取得可跨时物品';
  }
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
  const now = clock.elapsedTime;
  if (now - state.lastToggle < .55) return;
  setInventoryOpen(false);
  state.lastToggle = now;
  state.eraTarget = state.eraTarget > .5 ? 0 : 1;
  state.pulse = 1;
  document.body.classList.toggle('past', state.eraTarget < .5);
  eraLabel.textContent = state.eraTarget < .5 ? '过去 · 2047' : '现代 · 2147';
  flashTime();

  if (state.eraTarget < .5) {
    showToast('固定时间点：2047年，03号矿场仍在正常运行');
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
  state.inventoryOpen = false;
  state.inventory.handwheel = false;
  state.history.wheelCollected = false;
  state.history.wheelCrossed = false;
  state.history.wheelInstalled = false;
  state.history.gateOpened = false;
  inventoryPanel.classList.remove('open');
  inventoryPanel.setAttribute('aria-hidden', 'true');
  player.x = -10.4;
  player.y = groundY + player.halfH;
  player.vx = 0;
  player.vy = 0;
  document.body.classList.remove('past');
  eraLabel.textContent = '现代 · 2147';
  flashTime();
  showToast('时间线已重置');
  updateHud();
}

function overlaps(aCenter, aHalf, bCenter, bHalf) {
  return Math.abs(aCenter - bCenter) < aHalf + bHalf;
}

function updateHorizontal(dt) {
  const direction = state.inventoryOpen
    ? 0
    : (keys.has('KeyD') ? 1 : 0) - (keys.has('KeyA') ? 1 : 0);
  player.vx = THREE.MathUtils.damp(player.vx, direction * player.speed, direction ? 15 : 22, dt);
  if (direction) player.facing = direction;
  let nextX = THREE.MathUtils.clamp(player.x + player.vx * dt, -14.7, 14.7);
  const playerBottom = player.y - player.halfH;
  const playerTop = player.y + player.halfH;

  const gateBottom = groundY + (state.eraTarget < .5 ? 0 : state.gateLift * 5.2);
  const gateVerticalOverlap = playerTop > gateBottom + .05;
  if (gateVerticalOverlap && overlaps(nextX, player.halfW, gateX, .72)) {
    nextX = player.x < gateX
      ? gateX - .72 - player.halfW
      : gateX + .72 + player.halfW;
    player.vx = 0;
  }
  player.x = nextX;
}

function updateVertical(dt) {
  player.vy -= 24 * dt;
  let nextY = player.y + player.vy * dt;
  let landingY = groundY;

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
  if (state.inventoryOpen || !player.grounded) return;
  player.vy = player.jumpSpeed;
  player.grounded = false;
}

function handleInteraction() {
  if (state.inventoryOpen) return;
  const playerNearPickup = Math.abs(player.x - handwheelPickupX) < 1.75;
  const playerNearSocket = Math.abs(player.x - winchSocketX) < 2.0;

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
    showToast('验证完成：你把2047年的物品带到2147年解决了现代障碍');
    updateHud();
  }
}

function updateHud() {
  eventCrate.classList.toggle('active', state.history.wheelCollected);
  eventPlate.classList.toggle('active', state.history.wheelCrossed);
  eventGate.classList.toggle('active', state.history.gateOpened);
  eventCrate.querySelector('span').textContent = state.history.wheelCollected ? '手轮已从闸门拆下' : '尚未发生';
  eventPlate.querySelector('span').textContent = state.history.wheelCrossed ? '时间锚携带成功' : '等待背包';
  eventGate.querySelector('span').textContent = state.history.gateOpened ? '机械卡扣保持开启' : '等待物品';

  if (state.exitReached) {
    objective.textContent = '验证完成：时间锚背包把2047年的手轮带到了2147年';
  } else if (state.eraTarget < .5) {
    objective.textContent = state.history.wheelCollected
      ? '2047年：手轮已在时间锚背包中。按 B 查看，按 Q 把它带回2147年'
      : '2047年：闸门主电网在线且电磁锁定。靠近门边手轮按 E 拆下';
  } else if (state.history.gateOpened) {
    objective.textContent = '2147年：闸门已经升起并被机械卡扣锁住，前往右侧出口';
  } else if (state.history.wheelInstalled) {
    objective.textContent = '2147年：手轮已经装上。再次按 E 转动手轮并拉起闸门';
  } else if (state.inventory.handwheel) {
    objective.textContent = '2147年：2047年的手轮就在背包里。靠近门边接口按 E 安装';
  } else {
    objective.textContent = '2147年：闸门断电，但原手轮已经锈死。按 Q 查看2047年的同一机构';
  }
  doorStatus.classList.toggle('online', state.eraTarget < .5);
  if (state.eraTarget < .5) {
    doorPower.textContent = '主电网：在线 · 供电稳定';
    doorLock.textContent = state.history.wheelCollected
      ? '电磁联锁：锁定 · 应急手轮已拆下'
      : '电磁联锁：锁定 · 手轮无法驱动大门';
  } else {
    doorPower.textContent = '主电网：离线 · 无法恢复';
    if (state.history.gateOpened) doorLock.textContent = '机械卡扣：已锁定开启位置';
    else if (state.history.wheelInstalled) doorLock.textContent = '电磁联锁：失效 · 新手轮已安装';
    else if (state.inventory.handwheel) doorLock.textContent = '电磁联锁：失效 · 原接口已空置';
    else doorLock.textContent = '电磁联锁：失效 · 原手轮锈死';
  }
  updateInventoryHud();
}

function updateInteractionHint() {
  const nearPickup = Math.abs(player.x - handwheelPickupX) < 1.75;
  const nearSocket = Math.abs(player.x - winchSocketX) < 2.0;
  let message = '';

  if (state.inventoryOpen) {
    message = '';
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
  particleMaterial.color.copy(new THREE.Color('#ff9b52').lerp(new THREE.Color('#82d9e5'), state.era));
  particles.rotation.z = Math.sin(elapsed * .08) * .012;

  const playerPast = new THREE.Color('#ffd8ad');
  const playerPresent = new THREE.Color('#dffaff');
  const playerColor = playerPast.lerp(playerPresent, state.era);
  for (const item of playerMesh.userData.bodyMaterials) item.color.copy(playerColor);
  playerMesh.position.set(player.x, player.y, 2.2);
  playerMesh.scale.x = player.facing;
  playerMesh.rotation.z = THREE.MathUtils.damp(playerMesh.rotation.z, -player.vx * .008, 12, dt);

  const cameraShake = state.pulse * Math.sin(elapsed * 58) * .08;
  camera.position.x = cameraShake;
  camera.position.y = state.pulse * Math.cos(elapsed * 49) * .035;
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
  if (!event.repeat && event.code === 'KeyB') toggleInventory();
  if (!event.repeat && event.code === 'Escape' && state.inventoryOpen) setInventoryOpen(false);
  if (!event.repeat && event.code === 'KeyQ') toggleEra();
  if (!event.repeat && event.code === 'KeyR') resetHistory();
  if (!event.repeat && event.code === 'KeyE') handleInteraction();
  if (!event.repeat && (event.code === 'KeyW' || event.code === 'Space')) tryJump();
  keys.add(event.code);
  if (['Space', 'KeyW', 'KeyA', 'KeyB', 'KeyD', 'KeyE', 'KeyQ'].includes(event.code)) event.preventDefault();
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
  updateHorizontal(dt);
  updateVertical(dt);
  checkHistoryEvents();
  updateVisuals(dt, elapsed);
  updateHistoryOutcome(dt, elapsed);
  updateInteractionHint();
  renderer.render(scene, camera);
}

animate();
