import * as THREE from './vendor/three.module.js';

const canvas = document.querySelector('#game');
const eraLabel = document.querySelector('#era-label');
const objective = document.querySelector('#objective');
const bossHealthEl = document.querySelector('#boss-health');
const bossValueEl = document.querySelector('#boss-value');
const toast = document.querySelector('#toast');

const scene = new THREE.Scene();
const camera = new THREE.OrthographicCamera(-16, 16, 9, -9, 0.1, 100);
camera.position.z = 20;
const renderer = new THREE.WebGLRenderer({ canvas, antialias: true, alpha: false });
renderer.setPixelRatio(Math.min(devicePixelRatio, 2));
renderer.outputColorSpace = THREE.SRGBColorSpace;

const state = {
  era: 1,            // 0 = past, 1 = present
  eraTarget: 1,
  wallHp: 3,
  wallDestroyed: false,
  wallDestroyedAt: 0,
  presentCollapseStart: 0,
  bossHp: 100,
};

const keys = new Set();
const player = { x: -11, y: 0, facing: 1, speed: 7, radius: .55, attackUntil: 0 };
const clock = new THREE.Clock();

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
  varying vec2 vUv;
  float grid(vec2 p, float size) {
    vec2 g = abs(fract(p * size - .5) - .5) / fwidth(p * size);
    return 1.0 - min(min(g.x, g.y), 1.0);
  }
  void main() {
    vec3 past = vec3(.105, .018, .028);
    vec3 present = vec3(.025, .032, .043);
    vec3 color = mix(past, present, uEra);
    float line = grid(vUv + vec2(uTime * .003, 0.), 28.0) * .035;
    float glow = .04 / max(abs(vUv.y - .5), .035);
    color += mix(vec3(.32, .015, .025), vec3(.09, .13, .18), uEra) * (line + glow * .035);
    gl_FragColor = vec4(color, 1.0);
  }
`;

const backgroundMaterial = new THREE.ShaderMaterial({
  vertexShader, fragmentShader,
  uniforms: { uEra: { value: 1 }, uTime: { value: 0 } },
  depthWrite: false,
});
const background = new THREE.Mesh(new THREE.PlaneGeometry(40, 24), backgroundMaterial);
background.position.z = -10;
scene.add(background);

const pastColor = new THREE.Color('#ff3d50');
const presentColor = new THREE.Color('#e9f0f7');
const mutedPast = new THREE.Color('#681f2a');
const mutedPresent = new THREE.Color('#55606e');

function line(x1, y1, x2, y2, color = '#4b5360', opacity = .55) {
  const geometry = new THREE.BufferGeometry().setFromPoints([
    new THREE.Vector3(x1, y1, 0), new THREE.Vector3(x2, y2, 0),
  ]);
  const material = new THREE.LineBasicMaterial({ color, transparent: true, opacity });
  const object = new THREE.Line(geometry, material);
  scene.add(object);
  return object;
}

line(-18, -5.2, 18, -5.2);
line(-18, 5.2, 18, 5.2);
for (let x = -15; x <= 15; x += 3) line(x, -5.2, x, -4.95, '#59616d', .35);

function makeStickman() {
  const group = new THREE.Group();
  const material = new THREE.LineBasicMaterial({ color: presentColor });
  const points = [
    [-.33, .66, 0], [0, .82, 0], [.33, .66, 0], [.33, .35, 0], [0, .18, 0], [-.33, .35, 0], [-.33, .66, 0],
    [0, .18, 0], [0, -.55, 0], [-.42, -1.03, 0], [0, -.55, 0], [.42, -1.03, 0],
    [0, -.05, 0], [-.55, -.32, 0], [0, -.05, 0], [.55, -.32, 0],
  ].map(v => new THREE.Vector3(...v));
  const mesh = new THREE.Line(new THREE.BufferGeometry().setFromPoints(points), material);
  group.add(mesh);
  group.userData.material = material;
  return group;
}

const playerMesh = makeStickman();
scene.add(playerMesh);

const attackMaterial = new THREE.LineBasicMaterial({ color: presentColor, transparent: true, opacity: 0 });
const attackArc = new THREE.Line(
  new THREE.BufferGeometry().setFromPoints(Array.from({ length: 17 }, (_, i) => {
    const a = -1.15 + (i / 16) * 2.3;
    return new THREE.Vector3(Math.cos(a) * 1.15, Math.sin(a) * 1.15, 0);
  })), attackMaterial
);
playerMesh.add(attackArc);

const wallGroup = new THREE.Group();
const wallPieces = [];
for (let row = 0; row < 4; row++) {
  for (let col = 0; col < 2; col++) {
    const material = new THREE.MeshBasicMaterial({ color: presentColor, transparent: true, opacity: .78 });
    const mesh = new THREE.Mesh(new THREE.PlaneGeometry(1.32, 1.05), material);
    const base = new THREE.Vector3(col * 1.42 - .71, row * 1.15 - 1.72, 0);
    mesh.position.copy(base);
    wallGroup.add(mesh);
    wallPieces.push({
      mesh, material, base,
      fall: new THREE.Vector3(base.x + (col ? 2.5 : -2.0) + row * .25, -4.4 + row * .15, 0),
      rotation: (col ? -1 : 1) * (.35 + row * .22),
    });
  }
}
wallGroup.position.set(-1, 0, 0);
scene.add(wallGroup);

const bossMaterial = new THREE.MeshBasicMaterial({ color: presentColor, transparent: true, opacity: .16 });
const bossEdgesMaterial = new THREE.LineBasicMaterial({ color: presentColor });
const boss = new THREE.Group();
boss.add(new THREE.Mesh(new THREE.PlaneGeometry(3.3, 3.3), bossMaterial));
boss.add(new THREE.LineSegments(new THREE.EdgesGeometry(new THREE.PlaneGeometry(3.3, 3.3)), bossEdgesMaterial));
boss.position.set(10, 0, 0);
scene.add(boss);
line(8.35, -2.15, 11.65, -2.15, '#77818d', .6);

const eraObjects = [
  { mesh: boss, past: { x: 9.2, y: .45, r: -.08 }, present: { x: 10, y: 0, r: 0 } },
];

function showToast(message) {
  toast.textContent = message;
  toast.classList.add('show');
  clearTimeout(showToast.timer);
  showToast.timer = setTimeout(() => toast.classList.remove('show'), 1600);
}

function toggleEra() {
  state.eraTarget = state.eraTarget > .5 ? 0 : 1;
  if (state.eraTarget === 1 && state.wallDestroyed) {
    state.presentCollapseStart = clock.elapsedTime;
  }
  document.body.classList.toggle('past', state.eraTarget === 0);
  eraLabel.textContent = state.eraTarget === 0 ? '过去' : '现代';
  showToast(state.eraTarget === 0 ? '时间锚定：过去' : '时间锚定：现代');
}

function attack(now) {
  if (now < player.attackUntil - .12) return;
  player.attackUntil = now + .22;
  const wallDistance = Math.abs(player.x - wallGroup.position.x);
  if (state.eraTarget < .5 && !state.wallDestroyed && wallDistance < 2.2) {
    state.wallHp--;
    if (state.wallHp <= 0) {
      state.wallDestroyed = true;
      state.wallDestroyedAt = now;
      objective.textContent = '历史已改变。按 Q 回到现代观察结果';
      showToast('历史事件已写入：墙体摧毁');
    } else {
      showToast(`墙体结构受损 ${3 - state.wallHp}/3`);
    }
  }
  const bossDistance = Math.abs(player.x - boss.position.x) + Math.abs(player.y - boss.position.y);
  if (wallIsOpen() && bossDistance < 2.9) {
    state.bossHp = Math.max(0, state.bossHp - 10);
    bossHealthEl.style.width = `${state.bossHp}%`;
    bossValueEl.textContent = `${state.bossHp}%`;
    if (state.bossHp === 0) {
      objective.textContent = 'MVP 完成：你击败了守门者';
      showToast('BOX / 守门者 已击败');
    }
  }
}

function wallIsOpen() { return state.wallDestroyed; }

function updatePlayer(dt, now) {
  let dx = (keys.has('KeyD') ? 1 : 0) - (keys.has('KeyA') ? 1 : 0);
  let dy = (keys.has('KeyW') ? 1 : 0) - (keys.has('KeyS') ? 1 : 0);
  if (dx || dy) {
    const length = Math.hypot(dx, dy);
    dx /= length; dy /= length;
    if (dx) player.facing = Math.sign(dx);
  }
  let nextX = THREE.MathUtils.clamp(player.x + dx * player.speed * dt, -14.5, 14.5);
  let nextY = THREE.MathUtils.clamp(player.y + dy * player.speed * dt, -4.25, 4.25);

  const hitsClosedWall = !wallIsOpen() && Math.abs(nextX - wallGroup.position.x) < 1.35 && Math.abs(nextY) < 2.75;
  if (hitsClosedWall) nextX = player.x;
  const hitsBoss = state.bossHp > 0 && Math.abs(nextX - boss.position.x) < 2.05 && Math.abs(nextY - boss.position.y) < 2.05;
  if (hitsBoss) { nextX = player.x; nextY = player.y; }

  player.x = nextX; player.y = nextY;
  playerMesh.position.set(player.x, player.y, 1);
  playerMesh.scale.x = player.facing;
  attackMaterial.opacity = now < player.attackUntil ? Math.max(0, (player.attackUntil - now) / .22) : 0;
  attackArc.position.x = .85;
}

function updateEra(dt, elapsed) {
  state.era = THREE.MathUtils.damp(state.era, state.eraTarget, 8, dt);
  backgroundMaterial.uniforms.uEra.value = state.era;
  backgroundMaterial.uniforms.uTime.value = elapsed;
  const main = pastColor.clone().lerp(presentColor, state.era);
  const muted = mutedPast.clone().lerp(mutedPresent, state.era);
  playerMesh.userData.material.color.copy(main);
  attackMaterial.color.copy(main);
  bossMaterial.color.copy(main);
  bossEdgesMaterial.color.copy(main);
  bossMaterial.opacity = .10 + state.era * .08;
  for (const { mesh, past, present } of eraObjects) {
    mesh.position.x = THREE.MathUtils.lerp(past.x, present.x, state.era);
    mesh.position.y = THREE.MathUtils.lerp(past.y, present.y, state.era);
    mesh.rotation.z = THREE.MathUtils.lerp(past.r, present.r, state.era);
  }

  const collapseStart = state.eraTarget > .5 && state.presentCollapseStart
    ? state.presentCollapseStart
    : state.wallDestroyedAt;
  const breakProgress = state.wallDestroyed
    ? THREE.MathUtils.clamp((elapsed - collapseStart) / 1.1, 0, 1)
    : 0;
  for (let i = 0; i < wallPieces.length; i++) {
    const piece = wallPieces[i];
    const delayed = THREE.MathUtils.smoothstep(breakProgress, i * .035, .72 + i * .035);
    const targetProgress = state.wallDestroyed ? delayed : 0;
    piece.mesh.position.lerpVectors(piece.base, piece.fall, targetProgress);
    piece.mesh.rotation.z = piece.rotation * targetProgress;
    piece.material.color.copy(i % 2 ? main : muted);
    piece.material.opacity = state.wallDestroyed ? .78 - targetProgress * .38 : .72;
  }
  if (state.wallDestroyed && state.eraTarget > .5 && breakProgress > .9) {
    objective.textContent = '历史改变成功：穿过废墟，攻击右侧 BOX 守门者';
  }
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
  if (!event.repeat && event.code === 'Space') attack(clock.elapsedTime);
  keys.add(event.code);
  if (['Space', 'KeyW', 'KeyA', 'KeyS', 'KeyD'].includes(event.code)) event.preventDefault();
});
addEventListener('keyup', event => keys.delete(event.code));
addEventListener('blur', () => keys.clear());
addEventListener('resize', resize);
resize();

function animate() {
  requestAnimationFrame(animate);
  const dt = Math.min(clock.getDelta(), .05);
  const elapsed = clock.elapsedTime;
  updatePlayer(dt, elapsed);
  updateEra(dt, elapsed);
  boss.rotation.z = Math.sin(elapsed * 1.3) * .025;
  renderer.render(scene, camera);
}
animate();
