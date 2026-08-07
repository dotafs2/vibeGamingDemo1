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

function buildPastScene() {
  disc(pastLayer, 2.5, '#6f1f1d', 10.3, 3.4, -3, .34);
  disc(pastLayer, 1.72, '#d14f2d', 10.3, 3.4, -2.9, .18);

  rectangle(pastLayer, 7.2, 4.4, '#35191a', -8.8, -1.8, -2, .9);
  rectangle(pastLayer, 5.1, 3.2, '#4a2020', -2.2, -2.35, -2, .86);
  rectangle(pastLayer, 3.9, 5.8, '#2d1517', 8.3, -1.2, -2, .92);
  rectangle(pastLayer, 1.1, 7.5, '#3d1b1a', 12.8, -.75, -2, .92);
  rectangle(pastLayer, 2.1, .42, '#7b3927', 12.8, 2.75, -1.9, .8);

  for (let x = -11.3; x <= -6.3; x += 1.25) {
    rectangle(pastLayer, .12, 3.7, '#9e5232', x, -1.5, -1.6, .55);
  }
  for (let x = -4.1; x <= 0; x += 1.05) {
    rectangle(pastLayer, .10, 2.55, '#9e5232', x, -2.4, -1.6, .45);
  }

  segment(pastLayer, -12.2, .25, -5.4, .25, '#e1844d', .42);
  segment(pastLayer, -12, -3.2, -5.7, .1, '#9e5232', .32);
  segment(pastLayer, -11.5, .1, -5.7, -3.2, '#9e5232', .32);
  segment(pastLayer, -4.6, -1.1, .3, -1.1, '#dc7546', .35);

  for (let i = 0; i < 5; i++) {
    rectangle(pastLayer, .36, 1.5 + i * .22, '#6b2f25', 5.2 + i * .47, -3.7 + i * .11, -1.8, .78);
  }

  segment(pastLayer, 8.0, -4.15, 8.0, 1.8, '#bd6540', .38);
  segment(pastLayer, 9.1, -4.15, 9.1, 1.4, '#bd6540', .38);
  segment(pastLayer, 7.7, .9, 9.45, .9, '#df8150', .42);
  segment(pastLayer, 7.7, -.4, 9.45, -.4, '#df8150', .32);
}

function buildPresentScene() {
  disc(presentLayer, 2.4, '#397a83', -10.7, 3.7, -3, .16);
  disc(presentLayer, 1.75, '#8fdde4', -10.7, 3.7, -2.9, .08);

  rectangle(presentLayer, 6.8, 2.8, '#132d34', -9.0, -2.65, -2, .95);
  rectangle(presentLayer, 2.1, 4.1, '#17333a', -5.15, -2.0, -2, .88);
  rectangle(presentLayer, 4.8, 2.1, '#152c32', -1.9, -3.0, -2, .88);
  rectangle(presentLayer, 3.5, 3.2, '#10282f', 8.55, -2.4, -2, .92);
  rectangle(presentLayer, 1.25, 6.1, '#112c33', 12.5, -1.55, -2, .92);

  segment(presentLayer, -12.3, -1.2, -6.0, -.3, '#62adba', .18);
  segment(presentLayer, -11.6, -.5, -8.4, -3.6, '#62adba', .16);
  segment(presentLayer, -5.8, -.05, -4.2, -1.1, '#75bec8', .18);
  segment(presentLayer, -4.0, -1.95, .2, -1.35, '#75bec8', .16);
  segment(presentLayer, 8.7, -.8, 10.1, -3.55, '#75bec8', .18);

  for (let i = 0; i < 8; i++) {
    const x = -12 + i * 3.35;
    segment(presentLayer, x, groundY + .05, x + .35, groundY + .65 + (i % 3) * .25, '#5fa6ae', .22);
  }

  for (let i = 0; i < 6; i++) {
    rectangle(presentLayer, .22, 1.3 + i * .13, '#1d444b', 3.9 + i * .45, -3.85 + i * .07, -1.7, .8);
  }

  segment(presentLayer, 12.0, 1.5, 12.75, 2.35, '#77c6cf', .2);
  segment(presentLayer, 12.75, 2.35, 13.1, 1.5, '#77c6cf', .2);
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

function createCrate(width, height, fill, edge, rune = false) {
  const group = new THREE.Group();
  const fillMaterial = material(fill, .9);
  const edgeMaterial = lineMaterial(edge, .95);
  const face = new THREE.Mesh(new THREE.PlaneGeometry(width, height), fillMaterial);
  group.add(face);

  const outline = new THREE.LineLoop(
    new THREE.BufferGeometry().setFromPoints([
      new THREE.Vector3(-width / 2, -height / 2, .02),
      new THREE.Vector3(width / 2, -height / 2, .02),
      new THREE.Vector3(width / 2, height / 2, .02),
      new THREE.Vector3(-width / 2, height / 2, .02),
    ]),
    edgeMaterial,
  );
  group.add(outline);
  segment(group, -width * .38, -height * .38, width * .38, height * .38, edge, .62, .04);
  segment(group, -width * .38, height * .38, width * .38, -height * .38, edge, .62, .04);

  if (rune) {
    const points = Array.from({ length: 33 }, (_, index) => {
      const angle = index / 32 * Math.PI * 2;
      return new THREE.Vector3(Math.cos(angle) * width * .22, Math.sin(angle) * width * .22, .07);
    });
    group.add(new THREE.LineLoop(new THREE.BufferGeometry().setFromPoints(points), lineMaterial(edge, .78)));
    segment(group, 0, -height * .22, 0, height * .22, edge, .72, .08);
    segment(group, -width * .14, 0, width * .14, 0, edge, .72, .08);
  }

  group.userData.fillMaterial = fillMaterial;
  group.userData.edgeMaterial = edgeMaterial;
  return group;
}

const handwheelPickupX = -4.85;
const toolCratePast = createCrate(1.65, 1.28, '#6d3d22', '#ffb15d');
toolCratePast.position.set(handwheelPickupX, groundY + .64, 1.0);
scene.add(toolCratePast);

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

const pastHandwheelPickup = createHandwheel('#ffb15d');
pastHandwheelPickup.position.set(handwheelPickupX, groundY + 1.62, 1.3);
scene.add(pastHandwheelPickup);

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
      showToast('已拾取03号机械手轮；按 B 可以查看时间锚背包');
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
  eventCrate.querySelector('span').textContent = state.history.wheelCollected ? '手轮已进入背包' : '尚未发生';
  eventPlate.querySelector('span').textContent = state.history.wheelCrossed ? '时间锚携带成功' : '等待背包';
  eventGate.querySelector('span').textContent = state.history.gateOpened ? '机械卡扣保持开启' : '等待物品';

  if (state.exitReached) {
    objective.textContent = '验证完成：时间锚背包把2047年的手轮带到了2147年';
  } else if (state.eraTarget < .5) {
    objective.textContent = state.history.wheelCollected
      ? '2047年：手轮已在时间锚背包中。按 B 查看，按 Q 把它带回2147年'
      : '2047年：前往左侧橙色维修箱，靠近完整手轮后按 E 拾取';
  } else if (state.history.gateOpened) {
    objective.textContent = '2147年：闸门已经升起并被机械卡扣锁住，前往右侧出口';
  } else if (state.history.wheelInstalled) {
    objective.textContent = '2147年：手轮已经装上。再次按 E 转动手轮并拉起闸门';
  } else if (state.inventory.handwheel) {
    objective.textContent = '2147年：2047年的手轮就在背包里。靠近门边接口按 E 安装';
  } else {
    objective.textContent = '2147年：闸门缺少手动开门盘。按 Q 前往固定的2047年寻找零件';
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
      ? '维修箱上的机械手轮已经被你取走'
      : '按 E 拾取03号机械手轮并放入背包';
  } else if (state.eraTarget < .5 && nearSocket) {
    message = '2047年的电磁联锁仍在通电，手轮无法绕过权限';
  } else if (state.eraTarget > .5 && nearSocket) {
    if (!state.history.wheelInstalled && state.inventory.handwheel) message = '按 E 从背包取出手轮并安装';
    else if (!state.history.wheelInstalled) message = '2147年的手动开门接口缺少手轮';
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
  setLayerOpacity(toolCratePast, 1 - state.era);
  setLayerOpacity(pastHandwheelPickup, (1 - state.era) * (state.history.wheelCollected ? 0 : 1));
  setLayerOpacity(modernInstalledWheel, state.era * (state.history.wheelInstalled ? 1 : 0));

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
