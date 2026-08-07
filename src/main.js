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
  gateCollapse: 0,
  exitReached: false,
  history: {
    crateMoved: false,
    plateWeighted: false,
    gateSeen: false,
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

const pressureGroup = new THREE.Group();
pressureGroup.position.set(2.75, groundY + .13, .7);
scene.add(pressureGroup);
const pressureBaseMaterial = material('#5f3428', .95);
const pressureGlowMaterial = material('#ffaf5e', .35);
rectangle(pressureGroup, 2.3, .23, '#6c3527', 0, 0, 0, .95).material = pressureBaseMaterial;
rectangle(pressureGroup, 1.55, .08, '#ffad5a', 0, .16, .05, .42).material = pressureGlowMaterial;
segment(pressureGroup, -1.0, .3, -1.0, .8, '#f6a252', .38, .1);
segment(pressureGroup, 1.0, .3, 1.0, .8, '#f6a252', .38, .1);

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

const boxes = [
  {
    id: 'anchor',
    width: 1.55,
    height: 1.5,
    pastX: -1.65,
    presentX: -1.1,
    initialPastX: -1.65,
    initialPresentX: -1.1,
    presentTouched: false,
    pastMesh: createCrate(1.55, 1.5, '#7f3c24', '#ffb362', true),
    presentMesh: createCrate(1.55, 1.5, '#28515a', '#9ee9f2', true),
  },
  {
    id: 'ordinary',
    width: 1.18,
    height: 1.12,
    pastX: -7.15,
    presentX: -6.7,
    initialPastX: -7.15,
    initialPresentX: -6.7,
    presentTouched: false,
    pastMesh: createCrate(1.18, 1.12, '#68301f', '#e39152'),
    presentMesh: createCrate(1.18, 1.12, '#203f46', '#77c8d3'),
  },
];

for (const box of boxes) {
  scene.add(box.pastMesh, box.presentMesh);
}

const gate = new THREE.Group();
gate.position.set(7.35, groundY, 1);
scene.add(gate);
const gatePieces = [];
for (let index = 0; index < 4; index++) {
  const piece = new THREE.Group();
  const fill = rectangle(piece, .72, 4.8, '#21434b', 0, 2.4, 0, .94);
  const edge = new THREE.LineSegments(
    new THREE.EdgesGeometry(new THREE.PlaneGeometry(.72, 4.8)),
    lineMaterial('#98dce5', .72),
  );
  edge.position.y = 2.4;
  piece.add(edge);
  piece.position.x = (index - 1.5) * .74;
  piece.userData.fill = fill;
  piece.userData.edge = edge;
  gate.add(piece);
  gatePieces.push(piece);
}

const gateHeader = rectangle(gate, 3.85, .34, '#2a5660', 0, 5.02, .1, .92);
const gateWarning = rectangle(gate, 1.45, .2, '#9ae6ef', 0, 4.38, .2, .38);

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

function toggleEra() {
  const now = clock.elapsedTime;
  if (now - state.lastToggle < .55) return;
  state.lastToggle = now;
  state.eraTarget = state.eraTarget > .5 ? 0 : 1;
  state.pulse = 1;
  document.body.classList.toggle('past', state.eraTarget < .5);
  eraLabel.textContent = state.eraTarget < .5 ? '过去 · 2047' : '现代 · 2147';
  flashTime();

  if (state.eraTarget < .5) {
    showToast('时间坐标锁定：矿镇建造期 / 2047');
  } else if (state.history.plateWeighted) {
    state.history.gateSeen = true;
    showToast('历史结果载入：百年封锁结构已经坍塌');
  } else {
    showToast('返回现代：封锁门仍然存在');
  }
  updateHud();
}

function resetHistory() {
  state.eraTarget = 1;
  state.era = 1;
  state.pulse = 1;
  state.gateCollapse = 0;
  state.exitReached = false;
  state.history.crateMoved = false;
  state.history.plateWeighted = false;
  state.history.gateSeen = false;
  for (const box of boxes) {
    box.pastX = box.initialPastX;
    box.presentX = box.initialPresentX;
    box.presentTouched = false;
  }
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

function activeBoxX(box) {
  return state.eraTarget < .5 ? box.pastX : box.presentX;
}

function setActiveBoxX(box, value) {
  if (state.eraTarget < .5) {
    box.pastX = value;
    if (!box.presentTouched) box.presentX = value + .55;
    if (box.id === 'anchor' && Math.abs(box.pastX - box.initialPastX) > .3 && !state.history.crateMoved) {
      state.history.crateMoved = true;
      showToast('历史事件：时间箱的位置已被改写');
    }
  } else {
    box.presentX = value;
    box.presentTouched = true;
    if (box.id === 'anchor') showToast('现代移动只影响现代，不会反向改写过去');
  }
}

function overlaps(aCenter, aHalf, bCenter, bHalf) {
  return Math.abs(aCenter - bCenter) < aHalf + bHalf;
}

function boxCanMove(box, nextX) {
  if (nextX - box.width / 2 < -14.5 || nextX + box.width / 2 > 14.5) return false;
  for (const other of boxes) {
    if (other === box) continue;
    if (overlaps(nextX, box.width / 2, activeBoxX(other), other.width / 2)) return false;
  }
  const gateBlocking = state.eraTarget > .5 && state.gateCollapse < .62;
  if (gateBlocking && overlaps(nextX, box.width / 2, gate.position.x, 1.52)) return false;
  return true;
}

function updateHorizontal(dt) {
  const direction = (keys.has('KeyD') ? 1 : 0) - (keys.has('KeyA') ? 1 : 0);
  player.vx = THREE.MathUtils.damp(player.vx, direction * player.speed, direction ? 15 : 22, dt);
  if (direction) player.facing = direction;
  let nextX = THREE.MathUtils.clamp(player.x + player.vx * dt, -14.7, 14.7);
  const playerBottom = player.y - player.halfH;
  const playerTop = player.y + player.halfH;

  const orderedBoxes = [...boxes].sort((a, b) => direction >= 0 ? activeBoxX(a) - activeBoxX(b) : activeBoxX(b) - activeBoxX(a));
  for (const box of orderedBoxes) {
    const boxX = activeBoxX(box);
    const boxBottom = groundY;
    const boxTop = groundY + box.height;
    const verticalOverlap = playerTop > boxBottom + .08 && playerBottom < boxTop - .06;
    if (!verticalOverlap || !overlaps(nextX, player.halfW, boxX, box.width / 2)) continue;

    const pushAmount = nextX - player.x;
    const nextBoxX = boxX + pushAmount;
    if (direction && boxCanMove(box, nextBoxX)) {
      setActiveBoxX(box, nextBoxX);
    } else {
      nextX = direction > 0
        ? boxX - box.width / 2 - player.halfW
        : boxX + box.width / 2 + player.halfW;
      player.vx = 0;
    }
  }

  const gateBlocking = state.eraTarget > .5 && state.gateCollapse < .62;
  if (gateBlocking && overlaps(nextX, player.halfW, gate.position.x, 1.48) && playerBottom < groundY + 5.0) {
    nextX = player.x < gate.position.x
      ? gate.position.x - 1.48 - player.halfW
      : gate.position.x + 1.48 + player.halfW;
    player.vx = 0;
  }
  player.x = nextX;
}

function updateVertical(dt) {
  player.vy -= 24 * dt;
  const previousBottom = player.y - player.halfH;
  let nextY = player.y + player.vy * dt;
  let landingY = groundY;

  if (player.vy <= 0) {
    for (const box of boxes) {
      const top = groundY + box.height;
      const horizontalOverlap = overlaps(player.x, player.halfW * .82, activeBoxX(box), box.width / 2 * .92);
      const nextBottom = nextY - player.halfH;
      if (horizontalOverlap && previousBottom >= top - .08 && nextBottom <= top) {
        landingY = Math.max(landingY, top);
      }
    }
  }

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
  if (!player.grounded) return;
  player.vy = player.jumpSpeed;
  player.grounded = false;
}

function checkHistoryEvents() {
  const anchor = boxes[0];
  const anchorOnPlate = Math.abs(anchor.pastX - pressureGroup.position.x) < .66;
  if (state.eraTarget < .5 && anchorOnPlate && !state.history.plateWeighted) {
    state.history.plateWeighted = true;
    pressureGlowMaterial.opacity = .92;
    pressureGlowMaterial.userData.baseOpacity = .92;
    showToast('关键历史已锁定：时间压力板永久启动');
    updateHud();
  }

  if (
    state.eraTarget > .5
    && state.history.plateWeighted
    && state.gateCollapse > .75
    && player.x > 12.15
    && !state.exitReached
  ) {
    state.exitReached = true;
    showToast('验证完成：你通过改写历史打开了现代道路');
    updateHud();
  }
}

function updateHud() {
  eventCrate.classList.toggle('active', state.history.crateMoved);
  eventPlate.classList.toggle('active', state.history.plateWeighted);
  eventGate.classList.toggle('active', state.history.gateSeen);
  eventCrate.querySelector('span').textContent = state.history.crateMoved ? '过去位置已改写' : '尚未发生';
  eventPlate.querySelector('span').textContent = state.history.plateWeighted ? '历史机制已锁定' : '尚未发生';
  eventGate.querySelector('span').textContent = state.history.gateSeen ? '现代通路已开放' : '等待历史';

  if (state.exitReached) {
    objective.textContent = '原型验证完成：过去的行动已经为现代打开新道路';
  } else if (state.eraTarget < .5) {
    objective.textContent = state.history.plateWeighted
      ? '历史已记录。按 Q 返回现代查看封锁门的结果'
      : '把带圆形标记的时间箱推到右侧发光压力板上';
  } else if (state.history.plateWeighted) {
    objective.textContent = '历史改变成功：现代封锁门已坍塌，前往右侧出口';
  } else {
    objective.textContent = '现代封锁门仍然完整。按 Q 回到过去改变它的原因';
  }
}

function updateInteractionHint() {
  const nearest = boxes.reduce((best, box) => {
    const distance = Math.abs(player.x - activeBoxX(box));
    return !best || distance < best.distance ? { box, distance } : best;
  }, null);
  const show = nearest && nearest.distance < 1.65 && player.y < groundY + nearest.box.height + 1.0;
  interaction.classList.toggle('show', Boolean(show));
  if (show) {
    interaction.querySelector('span').textContent = state.eraTarget < .5
      ? '继续移动即可推动 · 过去的变化会传到现代'
      : '继续移动即可推动 · 现代变化不会反向传到过去';
  }
}

function updateGate(dt, elapsed) {
  const shouldCollapse = state.history.plateWeighted && state.eraTarget > .5;
  state.gateCollapse = THREE.MathUtils.damp(state.gateCollapse, shouldCollapse ? 1 : 0, 2.8, dt);

  for (let index = 0; index < gatePieces.length; index++) {
    const piece = gatePieces[index];
    const delay = index * .08;
    const progress = THREE.MathUtils.smoothstep(state.gateCollapse, delay, .7 + delay);
    const direction = index < 2 ? -1 : 1;
    piece.position.x = (index - 1.5) * .74 + direction * progress * (1.0 + index * .08);
    piece.position.y = -progress * (1.55 + (index % 2) * .35);
    piece.rotation.z = direction * progress * (.34 + index * .13);
  }
  gateHeader.position.y = 5.02 - state.gateCollapse * 4.3;
  gateHeader.rotation.z = state.gateCollapse * .22;
  gateWarning.material.opacity = .38 * (1 - state.gateCollapse);

  exitGlow.material.opacity = .035 + state.gateCollapse * .12 + Math.sin(elapsed * 3.1) * .012;
  exitCore.material.opacity = .18 + state.gateCollapse * .62;
}

function updateVisuals(dt, elapsed) {
  state.era = THREE.MathUtils.damp(state.era, state.eraTarget, 7.5, dt);
  state.pulse = Math.max(0, state.pulse - dt * 1.8);
  backgroundMaterial.uniforms.uEra.value = state.era;
  backgroundMaterial.uniforms.uTime.value = elapsed;
  backgroundMaterial.uniforms.uPulse.value = state.pulse;

  setLayerOpacity(pastLayer, 1 - state.era);
  setLayerOpacity(presentLayer, state.era);
  setLayerOpacity(pressureGroup, .18 + (1 - state.era) * .82);
  setLayerOpacity(gate, state.era);
  setLayerOpacity(exitGroup, state.era);

  const groundPast = new THREE.Color('#5d2b25');
  const groundPresent = new THREE.Color('#274048');
  groundMaterial.color.copy(groundPast.lerp(groundPresent, state.era));
  particleMaterial.color.copy(new THREE.Color('#ff9b52').lerp(new THREE.Color('#82d9e5'), state.era));
  particles.rotation.z = Math.sin(elapsed * .08) * .012;

  for (const box of boxes) {
    box.pastMesh.position.set(box.pastX, groundY + box.height / 2, 1.1);
    box.presentMesh.position.set(box.presentX, groundY + box.height / 2, 1.2);
    setLayerOpacity(box.pastMesh, 1 - state.era);
    setLayerOpacity(box.presentMesh, state.era);
    const nearPast = state.eraTarget < .5 && Math.abs(player.x - box.pastX) < 1.7;
    const nearPresent = state.eraTarget > .5 && Math.abs(player.x - box.presentX) < 1.7;
    box.pastMesh.userData.edgeMaterial.opacity = (nearPast ? 1 : .78) * (1 - state.era);
    box.presentMesh.userData.edgeMaterial.opacity = (nearPresent ? 1 : .78) * state.era;
  }

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
  if (!event.repeat && event.code === 'KeyQ') toggleEra();
  if (!event.repeat && event.code === 'KeyR') resetHistory();
  if (!event.repeat && (event.code === 'KeyW' || event.code === 'Space')) tryJump();
  keys.add(event.code);
  if (['Space', 'KeyW', 'KeyA', 'KeyD', 'KeyQ'].includes(event.code)) event.preventDefault();
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
  updateGate(dt, elapsed);
  updateVisuals(dt, elapsed);
  updateInteractionHint();
  renderer.render(scene, camera);
}

animate();
