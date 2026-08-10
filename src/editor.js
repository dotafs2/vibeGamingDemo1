import * as THREE from './vendor/three.module.js';
import './editor.css';

const DEFAULT_SCENE_ART = Object.freeze({
  enabled: true,
  independentComponents: true,
  pastImage: '/editor/scene-art/upper-room-2047.png',
  presentImage: '/editor/scene-art/upper-room-2147.png',
  x: -1,
  y: 1.15,
  width: 32,
  height: 16.1,
  opacity: 1,
});

const DEFAULT_ANIMATION = Object.freeze({
  type: 'none',
  speed: 45,
  phase: 0,
});

const DEFAULT_SCENE_COMPONENTS = Object.freeze([
  { id: 'scene-crusher', name: '投料斗与双辊破碎机', componentType: 'static', bounds: { minX: -16.5, maxX: -8.4, minY: -4.25, maxY: 6.6 } },
  { id: 'scene-conveyor', name: '矿石传送带', componentType: 'static', bounds: { minX: -9.2, maxX: -1.15, minY: -3.75, maxY: -.55 } },
  { id: 'scene-vent-housing', name: '通风机机壳', componentType: 'static', bounds: { minX: -2.2, maxX: 3.65, minY: 1.35, maxY: 7.45 } },
  { id: 'scene-maintenance', name: '中央维修平台', componentType: 'static', bounds: { minX: 2.55, maxX: 7.35, minY: -3.65, maxY: 1.55 } },
  { id: 'scene-gate', name: '03 号工业闸门', componentType: 'static', bounds: { minX: 7.15, maxX: 13.1, minY: -4.35, maxY: 3.45 } },
  { id: 'scene-shaft', name: '右侧升降井', componentType: 'static', bounds: { minX: 12.75, maxX: 15.25, minY: -4.5, maxY: 8.25 } },
  { id: 'scene-rails', name: '矿车轨道', componentType: 'static', bounds: { minX: -16.8, maxX: 15.1, minY: -5.25, maxY: -4.0 } },
  { id: 'scene-ceiling-cables', name: '顶部管线与岩层', componentType: 'static', bounds: { minX: -16.8, maxX: 15.1, minY: 5.65, maxY: 8.3 } },
]);

const COMPONENT_TYPE_LABELS = {
  animation: '会自动运动',
  physics: '有物理、可破坏',
  static: '普通场景物',
};

const DEFAULT_STATE = Object.freeze({
  schemaVersion: 3,
  references: { past: [], present: [] },
  sceneArt: DEFAULT_SCENE_ART,
  sceneComponents: DEFAULT_SCENE_COMPONENTS,
  assets: [],
  editor: { gridSize: 0.5, snapEnabled: true },
});

const PHYSICS_LABELS = {
  pushable: '玩家可推动',
  'static-collider': '带碰撞的静态物体',
  'static-visual': '无碰撞的静态物体',
};

const ANIMATION_LABELS = {
  none: '无固定动画',
  rotate: '持续旋转',
};

function inferComponentType(asset) {
  if (asset.componentType && COMPONENT_TYPE_LABELS[asset.componentType]) return asset.componentType;
  if (asset.animation?.type && asset.animation.type !== 'none') return 'animation';
  if (asset.physicsType && asset.physicsType !== 'static-visual') return 'physics';
  return 'static';
}

function clone(value) {
  return JSON.parse(JSON.stringify(value));
}

function clamp(value, min, max) {
  return Math.min(max, Math.max(min, value));
}

function escapeHtml(value) {
  return String(value ?? '')
    .replaceAll('&', '&amp;')
    .replaceAll('<', '&lt;')
    .replaceAll('>', '&gt;')
    .replaceAll('"', '&quot;')
    .replaceAll("'", '&#039;');
}

function setNested(target, path, value) {
  const parts = path.split('.');
  let cursor = target;
  for (let index = 0; index < parts.length - 1; index++) cursor = cursor[parts[index]];
  cursor[parts.at(-1)] = value;
}

function readFileAsDataUrl(file) {
  return new Promise((resolve, reject) => {
    const reader = new FileReader();
    reader.onload = () => resolve(reader.result);
    reader.onerror = () => reject(reader.error || new Error('读取图片失败'));
    reader.readAsDataURL(file);
  });
}

function loadImage(url) {
  return new Promise((resolve, reject) => {
    const image = new Image();
    image.decoding = 'async';
    image.onload = () => resolve(image);
    image.onerror = () => reject(new Error(`无法加载图片：${url}`));
    image.src = url;
  });
}

function createDefaultAsset(bounds, description, sourceIds) {
  const timestamp = Date.now();
  return {
    id: `asset-${timestamp}-${Math.random().toString(36).slice(2, 7)}`,
    name: description.trim().slice(0, 32) || '未命名场景组件',
    description: description.trim(),
    sourceIds,
    sourceComponentId: null,
    x: (bounds.minX + bounds.maxX) * 0.5,
    y: (bounds.minY + bounds.maxY) * 0.5,
    width: Math.max(0.25, bounds.maxX - bounds.minX),
    height: Math.max(0.25, bounds.maxY - bounds.minY),
    rotation: 0,
    opacity: 1,
    depth: 0,
    componentType: 'static',
    renderMode: 'source-png',
    physicsType: 'static-visual',
    fracturePieces: 4,
    animation: clone(DEFAULT_ANIMATION),
    collider: {
      offsetX: 0,
      offsetY: 0,
      width: Math.max(0.25, bounds.maxX - bounds.minX),
      height: Math.max(0.25, bounds.maxY - bounds.minY),
    },
    versions: [],
    activeVersion: 0,
    pendingJobId: null,
    createdAt: new Date().toISOString(),
  };
}

function normalizeAsset(asset) {
  const normalized = {
    ...asset,
    sourceIds: {
      past: asset.sourceIds?.past || [],
      present: asset.sourceIds?.present || [],
    },
    renderMode: 'source-png',
    collider: {
      offsetX: 0,
      offsetY: 0,
      width: asset.width || 1,
      height: asset.height || 1,
      ...(asset.collider || {}),
    },
    versions: asset.versions || [],
    activeVersion: clamp(asset.activeVersion || 0, 0, Math.max(0, (asset.versions || []).length - 1)),
    physicsType: asset.physicsType || 'static-visual',
    fracturePieces: clamp(Math.round(Number(asset.fracturePieces) || 4), 4, 8),
    animation: { ...DEFAULT_ANIMATION, ...(asset.animation || {}) },
    visible: asset.visible ?? true,
  };
  normalized.componentType = inferComponentType(normalized);
  return normalized;
}

function normalizeState(value) {
  return {
    schemaVersion: 3,
    references: {
      past: value?.references?.past || [],
      present: value?.references?.present || [],
    },
    sceneArt: { ...DEFAULT_SCENE_ART, ...(value?.sceneArt || {}) },
    sceneComponents: clone(value?.sceneComponents || DEFAULT_SCENE_COMPONENTS),
    assets: (value?.assets || []).map(normalizeAsset),
    editor: {
      gridSize: value?.editor?.gridSize || 0.5,
      snapEnabled: value?.editor?.snapEnabled ?? true,
    },
  };
}

function createEditorDom() {
  const entry = document.createElement('button');
  entry.className = 'editor-entry';
  entry.type = 'button';
  entry.setAttribute('aria-pressed', 'false');
  entry.textContent = 'EDITOR';
  document.body.append(entry);

  const shell = document.createElement('section');
  shell.className = 'asset-editor';
  shell.setAttribute('aria-label', '场景资产编辑器');
  shell.innerHTML = `
    <header class="editor-topbar">
      <div class="editor-title"><b>场景编辑器</b><span>点击、拖动、圈选后告诉 Codex</span></div>
      <button class="editor-tool active" type="button" data-tool="select">移动物体</button>
      <button class="editor-tool" type="button" data-tool="box-select">圈选已有</button>
      <button class="editor-tool editor-add-tool" type="button" data-tool="box-add">＋ 添加新物体</button>
      <button class="editor-button" id="editor-reference-toggle" type="button">画风参考</button>
      <details class="editor-global-settings">
        <summary>设置</summary>
        <div class="editor-global-settings-popover">
          <label class="editor-scene-toggle"><input id="editor-scene-art-toggle" type="checkbox" checked> 显示整景美术</label>
          <div class="editor-grid-setting">
            <label><input id="editor-snap" type="checkbox" checked> 移动时吸附网格</label>
            <label>网格大小 <input id="editor-grid-size" type="number" min="0.05" max="4" step="0.05" value="0.5" aria-label="网格尺寸"></label>
          </div>
        </div>
      </details>
      <button class="editor-button primary" id="editor-save-play" type="button">保存并试玩</button>
    </header>
    <aside class="editor-reference-board" id="editor-reference-board">
      <div class="reference-board-header"><b>画风参考</b><button class="editor-small-button" id="editor-reference-close" type="button">完成</button></div>
      <p class="reference-board-help">Codex 会自动读取这里的图片来保持整个场景画风一致。</p>
      <div class="reference-columns">
        <div class="reference-column"><strong>2047 · 完整状态</strong><div class="reference-strip" id="editor-past-references"></div></div>
        <div class="reference-column"><strong>2147 · 破败状态</strong><div class="reference-strip" id="editor-present-references"></div></div>
      </div>
    </aside>
    <aside class="editor-inspector" id="editor-inspector"></aside>
  `;
  document.body.append(shell);

  const selectionRect = document.createElement('div');
  selectionRect.className = 'editor-selection-rect';
  selectionRect.dataset.label = '圈选区域';
  document.body.append(selectionRect);

  const regionComposer = document.createElement('form');
  regionComposer.className = 'editor-region-composer';
  regionComposer.innerHTML = `
    <label for="editor-region-prompt">这个框里想添加什么？</label>
    <textarea id="editor-region-prompt" placeholder="例如：一台低矮的旧工业风扇，叶片一直缓慢转动"></textarea>
    <div class="editor-region-composer-actions">
      <button class="editor-small-button" type="button" data-region-cancel>取消</button>
      <button class="editor-button primary" type="submit">复制给 Codex</button>
    </div>
    <small>位置、大小、现场截图和画风参考会自动放进提示词。</small>
  `;
  document.body.append(regionComposer);

  const modeGuide = document.createElement('div');
  modeGuide.className = 'editor-mode-guide';
  document.body.append(modeGuide);

  const assetBox = document.createElement('div');
  assetBox.className = 'editor-asset-box';
  document.body.append(assetBox);

  const componentLayer = document.createElement('div');
  componentLayer.className = 'editor-component-layer';
  document.body.append(componentLayer);

  const versionSwitcher = document.createElement('div');
  versionSwitcher.className = 'editor-version-switcher';
  versionSwitcher.innerHTML = `
    <button class="editor-small-button" type="button" data-version="previous">←</button>
    <span>0 / 0</span>
    <button class="editor-small-button" type="button" data-version="next">→</button>
  `;
  document.body.append(versionSwitcher);

  const status = document.createElement('div');
  status.className = 'editor-status';
  status.setAttribute('role', 'status');
  document.body.append(status);

  return {
    entry,
    shell,
    selectionRect,
    regionComposer,
    regionPrompt: regionComposer.querySelector('#editor-region-prompt'),
    modeGuide,
    assetBox,
    componentLayer,
    versionSwitcher,
    status,
    inspector: shell.querySelector('#editor-inspector'),
    pastReferences: shell.querySelector('#editor-past-references'),
    presentReferences: shell.querySelector('#editor-present-references'),
    snap: shell.querySelector('#editor-snap'),
    gridSize: shell.querySelector('#editor-grid-size'),
    sceneArtToggle: shell.querySelector('#editor-scene-art-toggle'),
    referenceBoard: shell.querySelector('#editor-reference-board'),
    referenceToggle: shell.querySelector('#editor-reference-toggle'),
    referenceClose: shell.querySelector('#editor-reference-close'),
    savePlay: shell.querySelector('#editor-save-play'),
  };
}

function numberInput(label, field, value, min, max, step) {
  return `<div class="editor-field"><label>${label}</label><input type="number" data-field="${field}" value="${Number(value).toFixed(2)}" min="${min}" max="${max}" step="${step}"></div>`;
}

function rangeInput(label, field, value, min, max, step, suffix = '') {
  return `<div class="editor-range-row"><label>${label}</label><input type="range" data-field="${field}" value="${value}" min="${min}" max="${max}" step="${step}"><output>${value}${suffix}</output></div>`;
}

function physicsSelect(value, draft = false) {
  const field = draft ? 'physicsType' : 'physicsType';
  const options = Object.entries(PHYSICS_LABELS)
    .map(([id, label]) => `<option value="${id}" ${id === value ? 'selected' : ''}>${label}</option>`)
    .join('');
  return `<div class="editor-field"><label>物理类型</label><select data-field="${field}">${options}</select></div>`;
}

function animationSelect(value) {
  const options = Object.entries(ANIMATION_LABELS)
    .map(([id, label]) => `<option value="${id}" ${id === value ? 'selected' : ''}>${label}</option>`)
    .join('');
  return `<div class="editor-field"><label>固定动画</label><select data-field="animation.type">${options}</select></div>`;
}

function componentTypeSelect(value) {
  const options = Object.entries(COMPONENT_TYPE_LABELS)
    .map(([id, label]) => `<option value="${id}" ${id === value ? 'selected' : ''}>${label}</option>`)
    .join('');
  return `<div class="editor-field"><label>这个物体</label><select data-field="componentType">${options}</select></div>`;
}

function applyComponentType(target, type) {
  target.componentType = type;
  if (type === 'animation') {
    target.physicsType = 'static-visual';
    if (!target.animation) target.animation = clone(DEFAULT_ANIMATION);
    if (target.animation.type === 'none') target.animation.type = 'rotate';
    if (!Number.isFinite(Number(target.animation.speed))) target.animation.speed = 45;
  } else if (type === 'physics') {
    target.physicsType = target.physicsType === 'static-visual' ? 'pushable' : target.physicsType;
    target.animation = { ...DEFAULT_ANIMATION, ...(target.animation || {}), type: 'none' };
  } else {
    target.physicsType = 'static-visual';
    target.animation = { ...DEFAULT_ANIMATION, ...(target.animation || {}), type: 'none' };
  }
}

export function createAssetEditor({
  scene,
  camera,
  renderer,
  canvas,
  pastLayer,
  presentLayer,
  getEra,
  getGroundForY,
}) {
  const elements = createEditorDom();
  const sceneArtRoot = new THREE.Group();
  sceneArtRoot.name = 'full-scene-art';
  scene.add(sceneArtRoot);
  const assetRoot = new THREE.Group();
  assetRoot.name = 'generated-editor-assets';
  scene.add(assetRoot);

  const raycaster = new THREE.Raycaster();
  const pointerNdc = new THREE.Vector2();
  const whiteboxObjects = new Map();
  const assetGroups = new Map();
  const assetMeshes = [];
  const imageCache = new Map();
  const textureGeneration = new Map();
  const pushableVelocities = new Map();
  const animationPhases = new Map();
  const fracturedAssets = new Set();
  const fractureRecords = new Map();

  let sceneArtRecord = {
    pastBackdrop: null,
    presentBackdrop: null,
    pastMesh: null,
    presentMesh: null,
  };

  let editorState = clone(DEFAULT_STATE);
  let active = false;
  let tool = 'select';
  let selectedAssetId = null;
  let copiedAsset = null;
  let pasteCount = 0;
  let draft = null;
  let boxDrag = null;
  let assetDrag = null;
  let saveTimer = 0;
  let statusTimer = 0;
  let pollTimer = 0;
  let physicsSaveTimer = 0;

  function showStatus(message, error = false) {
    elements.status.textContent = message;
    elements.status.classList.toggle('error', error);
    elements.status.classList.add('show');
    clearTimeout(statusTimer);
    statusTimer = setTimeout(() => elements.status.classList.remove('show'), error ? 4200 : 2600);
  }

  function snapValue(value) {
    if (!editorState.editor.snapEnabled) return value;
    const grid = Math.max(0.01, Number(editorState.editor.gridSize) || 0.5);
    return Math.round(value / grid) * grid;
  }

  function registerWhiteboxLayer(layer, era) {
    let index = 0;
    layer.traverse(object => {
      if (!(object.isMesh || object.isLine || object.isLineLoop)) return;
      const id = `${era}-${String(index).padStart(5, '0')}`;
      index += 1;
      object.userData.editorWhiteboxId = id;
      object.userData.editorWhiteboxEra = era;
      object.userData.editorInitialVisibility = object.visible;
      whiteboxObjects.set(id, object);
    });
  }

  registerWhiteboxLayer(pastLayer, 'past');
  registerWhiteboxLayer(presentLayer, 'present');

  function findAsset(id = selectedAssetId) {
    return editorState.assets.find(asset => asset.id === id) || null;
  }

  function boundsForAsset(asset) {
    return {
      minX: asset.x - Math.abs(asset.width) * .5,
      maxX: asset.x + Math.abs(asset.width) * .5,
      minY: asset.y - Math.abs(asset.height) * .5,
      maxY: asset.y + Math.abs(asset.height) * .5,
    };
  }

  function componentCatalog() {
    const extractedSceneIds = new Set(editorState.assets.map(asset => asset.sourceComponentId).filter(Boolean));
    const sceneDefinitions = editorState.sceneArt.independentComponents ? [] : editorState.sceneComponents;
    const sceneItems = sceneDefinitions
      .filter(component => !extractedSceneIds.has(component.id))
      .map(component => ({
        ...component,
        kind: 'scene',
        componentType: component.componentType || 'static',
      }));
    const assetItems = editorState.assets.map(asset => ({
      id: asset.id,
      name: asset.name,
      kind: 'asset',
      componentType: inferComponentType(asset),
      bounds: boundsForAsset(asset),
      asset,
    }));
    return [...sceneItems, ...assetItems];
  }

  function boundsIntersect(a, b) {
    return a.minX <= b.maxX && a.maxX >= b.minX && a.minY <= b.maxY && a.maxY >= b.minY;
  }

  function componentsInside(bounds) {
    return componentCatalog().filter(component => boundsIntersect(bounds, component.bounds));
  }

  function renderComponentMap() {
    elements.componentLayer.innerHTML = componentCatalog().map(component => `
      <div class="editor-component-box kind-${component.componentType}" data-component-box="${escapeHtml(component.id)}">
        <span>${escapeHtml(COMPONENT_TYPE_LABELS[component.componentType])} · ${escapeHtml(component.name)}</span>
      </div>
    `).join('');
    updateComponentMap();
  }

  function updateComponentMap() {
    if (!active) return;
    const catalog = new Map(componentCatalog().map(component => [component.id, component]));
    elements.componentLayer.querySelectorAll('[data-component-box]').forEach(element => {
      const component = catalog.get(element.dataset.componentBox);
      if (!component) {
        element.style.display = 'none';
        return;
      }
      const rect = rectFromWorldBounds(component.bounds);
      const visible = rect.right >= 0 && rect.left <= innerWidth && rect.bottom >= 54 && rect.top <= innerHeight;
      setRectElement(element, rect, visible);
      element.classList.toggle('selected', component.id === selectedAssetId || component.id === draft?.selectedComponentId);
    });
  }

  function worldFromClient(clientX, clientY) {
    const rect = canvas.getBoundingClientRect();
    const point = new THREE.Vector3(
      ((clientX - rect.left) / rect.width) * 2 - 1,
      -((clientY - rect.top) / rect.height) * 2 + 1,
      0,
    );
    point.unproject(camera);
    return point;
  }

  function clientFromWorld(x, y) {
    const rect = canvas.getBoundingClientRect();
    const point = new THREE.Vector3(x, y, 0).project(camera);
    return {
      x: rect.left + (point.x + 1) * 0.5 * rect.width,
      y: rect.top + (-point.y + 1) * 0.5 * rect.height,
    };
  }

  function rectFromWorldBounds(bounds) {
    const topLeft = clientFromWorld(bounds.minX, bounds.maxY);
    const bottomRight = clientFromWorld(bounds.maxX, bounds.minY);
    return {
      left: Math.min(topLeft.x, bottomRight.x),
      top: Math.min(topLeft.y, bottomRight.y),
      right: Math.max(topLeft.x, bottomRight.x),
      bottom: Math.max(topLeft.y, bottomRight.y),
      width: Math.abs(bottomRight.x - topLeft.x),
      height: Math.abs(bottomRight.y - topLeft.y),
    };
  }

  function setRectElement(element, rect, visible = true) {
    element.style.display = visible ? 'block' : 'none';
    if (!visible) return;
    element.style.left = `${rect.left}px`;
    element.style.top = `${rect.top}px`;
    element.style.width = `${Math.max(1, rect.width)}px`;
    element.style.height = `${Math.max(1, rect.height)}px`;
  }

  function collectWhiteboxIds(layer, era, screenRect) {
    const ids = [];
    const bounds = new THREE.Box3();
    const center = new THREE.Vector3();
    const min = new THREE.Vector3();
    const max = new THREE.Vector3();

    layer.traverse(object => {
      if (!object.userData.editorWhiteboxId || object.userData.editorLocked) return;
      bounds.setFromObject(object);
      if (bounds.isEmpty()) return;
      bounds.getCenter(center);
      const centerClient = clientFromWorld(center.x, center.y);
      if (centerClient.x < screenRect.left || centerClient.x > screenRect.right
        || centerClient.y < screenRect.top || centerClient.y > screenRect.bottom) return;

      min.copy(bounds.min);
      max.copy(bounds.max);
      const projectedMin = clientFromWorld(min.x, min.y);
      const projectedMax = clientFromWorld(max.x, max.y);
      const projectedWidth = Math.abs(projectedMax.x - projectedMin.x);
      const projectedHeight = Math.abs(projectedMax.y - projectedMin.y);
      if (projectedWidth > screenRect.width * 2.6 || projectedHeight > screenRect.height * 2.6) return;
      ids.push(`${era}-${object.userData.editorWhiteboxId.split('-').at(-1)}`);
    });
    return ids;
  }

  function refreshWhiteboxVisibility() {
    const hidden = new Set();
    for (const asset of editorState.assets) {
      if (!asset.versions.length) continue;
      for (const id of asset.sourceIds?.past || []) hidden.add(id);
      for (const id of asset.sourceIds?.present || []) hidden.add(id);
    }
    for (const [id, object] of whiteboxObjects) {
      object.userData.editorReplaced = hidden.has(id);
      if (object.userData.editorReplaced) object.visible = false;
      else if (object.visible === false && object.userData.editorInitialVisibility !== false) object.visible = true;
    }
  }

  function enforceHiddenWhitebox() {
    for (const object of whiteboxObjects.values()) {
      if (object.userData.editorReplaced) object.visible = false;
    }
  }

  async function loadEditorState() {
    try {
      let response = await fetch('/api/editor/state');
      if (!response.ok) response = await fetch('/editor/editor-state.json');
      if (!response.ok) throw new Error('无法读取编辑器数据');
      editorState = normalizeState(await response.json());
    } catch (error) {
      editorState = normalizeState(DEFAULT_STATE);
      showStatus(`${error.message}，已使用空白编辑器数据`, true);
    }
    elements.snap.checked = editorState.editor.snapEnabled;
    elements.gridSize.value = editorState.editor.gridSize;
    elements.sceneArtToggle.checked = editorState.sceneArt.enabled;
    sceneArtRoot.visible = editorState.sceneArt.enabled;
    renderReferences();
    await rebuildSceneArt();
    rebuildAllAssets();
    renderComponentMap();
    refreshWhiteboxVisibility();
    renderInspector();
    startPolling();
  }

  async function saveStateNow() {
    clearTimeout(saveTimer);
    const response = await fetch('/api/editor/state', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(editorState),
    });
    if (!response.ok) {
      const result = await response.json().catch(() => ({}));
      throw new Error(result.error || '保存编辑器数据失败');
    }
  }

  function saveStateSoon() {
    clearTimeout(saveTimer);
    saveTimer = setTimeout(() => saveStateNow().catch(error => showStatus(error.message, true)), 420);
  }

  function renderReferences() {
    const renderSlot = (slot, container) => {
      container.innerHTML = editorState.references[slot].map(reference => `
        <figure class="reference-thumb">
          <img src="${escapeHtml(reference.url)}" alt="${escapeHtml(reference.name)}">
          <button type="button" data-remove-reference="${escapeHtml(reference.id)}" data-slot="${slot}" aria-label="删除参考图">×</button>
          <small>${escapeHtml(reference.name)}</small>
        </figure>
      `).join('') + `<button class="reference-drop" type="button" data-reference-drop="${slot}">拖入或点击<br>添加参考图</button>`;
    };
    renderSlot('past', elements.pastReferences);
    renderSlot('present', elements.presentReferences);
  }

  async function uploadReferences(slot, files) {
    const images = [...files].filter(file => file.type.startsWith('image/'));
    if (!images.length) {
      showStatus('请拖入 PNG、JPEG 或 WebP 图片', true);
      return;
    }
    showStatus(`正在保存 ${images.length} 张参考图…`);
    for (const file of images) {
      const dataUrl = await readFileAsDataUrl(file);
      const response = await fetch('/api/editor/reference', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ slot, name: file.name, dataUrl }),
      });
      const result = await response.json();
      if (!response.ok) throw new Error(result.error || '保存参考图失败');
      editorState.references[slot].push(result);
    }
    renderReferences();
    await saveStateNow();
    showStatus('参考图已保存并会显示在后续 Codex 任务中');
  }

  function buildReferenceFileInput(slot) {
    const input = document.createElement('input');
    input.type = 'file';
    input.accept = 'image/png,image/jpeg,image/webp';
    input.multiple = true;
    input.addEventListener('change', () => {
      uploadReferences(slot, input.files).catch(error => showStatus(error.message, true));
      input.remove();
    }, { once: true });
    input.click();
  }

  function renderInspector() {
    if (draft?.candidates?.length && !draft.selectedComponentId) {
      elements.inspector.innerHTML = `
        <header class="editor-inspector-header">
          <span>COMPONENTS IN REGION</span>
          <h2>选择要调整的组件</h2>
          <p>框内有 ${draft.candidates.length} 个物体，点一个继续调整。</p>
        </header>
        <section class="editor-section">
          <div class="editor-component-choices">
            ${draft.candidates.map(component => `
              <button class="editor-component-choice kind-${component.componentType}" type="button" data-action="choose-component" data-component-id="${escapeHtml(component.id)}">
                <b>${escapeHtml(component.name)}</b>
                <span>${escapeHtml(COMPONENT_TYPE_LABELS[component.componentType])} · ${component.kind === 'asset' ? '已有独立组件' : '整景内组件'}</span>
              </button>
            `).join('')}
          </div>
        </section>
        <section class="editor-section">
          <div class="editor-actions">
            <button class="editor-button" type="button" data-action="use-drawn-region">把手动画框作为新组件</button>
            <button class="editor-button" type="button" data-action="cancel-draft">取消</button>
          </div>
        </section>
      `;
      return;
    }

    if (draft) {
      const pastCount = draft.sourceIds.past.length;
      const presentCount = draft.sourceIds.present.length;
      const addingNewComponent = draft.intent === 'add';
      if (addingNewComponent) {
        elements.inspector.innerHTML = `
          <div class="editor-empty editor-simple-help">
            <b>位置已经选好了</b>
            <p>直接在框旁边输入你想添加的物体，然后点“复制给 Codex”。</p>
            <p>位置、尺寸、现场截图、两个时代和整体画风都会自动处理，不需要填写技术参数。</p>
          </div>
        `;
        return;
      }
      elements.inspector.innerHTML = `
        <header class="editor-inspector-header">
          <span>新组件区域</span>
          <h2>告诉 Codex 这是什么</h2>
          <p>已识别 ${pastCount} / ${presentCount} 个时代几何片段</p>
        </header>
        <section class="editor-section">
          <div class="editor-field stack"><label>它是什么？</label><textarea data-draft-field="description" placeholder="例如：旧工业液压泵，接口朝右">${escapeHtml(draft.description)}</textarea></div>
        </section>
        <section class="editor-section">
          <div class="editor-actions">
            <button class="editor-button primary" type="button" data-action="create-job">复制给 Codex</button>
            <button class="editor-button" type="button" data-action="cancel-draft">取消</button>
          </div>
          <p class="editor-job-note">只处理这个框里的组件；完成后会自动回到原位置。</p>
        </section>
      `;
      return;
    }

    const asset = findAsset();
    if (!asset) {
      elements.inspector.innerHTML = `
        <div class="editor-empty editor-simple-help">
          <b>你想做什么？</b>
          <p><strong>移动：</strong>点“移动物体”，直接拖动物体。</p>
          <p><strong>添加：</strong>点“添加新物体”，在场景中拖一个框，然后说一句想放什么。</p>
          <p><strong>调整：</strong>点中物体后，在这里复制、删除或切换版本。</p>
        </div>
      `;
      return;
    }

    const versionCount = asset.versions.length;
    const activeVersion = versionCount ? asset.activeVersion + 1 : 0;
    elements.inspector.innerHTML = `
      <header class="editor-inspector-header">
        <span>已选中 · 版本 ${activeVersion}/${versionCount}</span>
        <h2>${escapeHtml(asset.name)}</h2>
        <p>${asset.pendingJobId ? 'Codex 正在处理这个物体' : COMPONENT_TYPE_LABELS[asset.componentType]}</p>
      </header>
      <section class="editor-section">
        <div class="editor-field"><label>名称</label><input type="text" data-field="name" value="${escapeHtml(asset.name)}"></div>
        ${componentTypeSelect(asset.componentType)}
        <div class="editor-actions">
          <button class="editor-button" type="button" data-action="duplicate">复制</button>
          <button class="editor-button danger" type="button" data-action="delete">删除</button>
        </div>
      </section>
      <section class="editor-section">
        <h3>历史版本</h3>
        <div class="editor-actions">
          <button class="editor-button" type="button" data-action="version-previous">上一版</button>
          <button class="editor-button" type="button" data-action="version-next">下一版</button>
          <button class="editor-button" type="button" data-action="regenerate">让 Codex 重做</button>
        </div>
        <p class="editor-job-note ${asset.pendingJobId ? 'pending' : ''}">${asset.pendingJobId ? `等待任务 ${escapeHtml(asset.pendingJobId)}` : `已保留 ${versionCount} 套双时代历史版本`}</p>
      </section>
      <section class="editor-section">
        <details class="editor-advanced">
          <summary>更多调整</summary>
          <div class="editor-advanced-content">
            <div class="editor-field stack"><label>告诉 Codex 这个物体是什么</label><textarea data-field="description">${escapeHtml(asset.description)}</textarea></div>
            <div class="editor-field"><label>显示物体</label><input type="checkbox" data-field="visible" ${asset.visible ? 'checked' : ''}></div>
            <div class="editor-inline-fields">
              ${numberInput('位置 X', 'x', asset.x, -200, 200, 0.05)}
              ${numberInput('位置 Y', 'y', asset.y, -100, 100, 0.05)}
              ${numberInput('宽度', 'width', asset.width, 0.1, 100, 0.05)}
              ${numberInput('高度', 'height', asset.height, 0.1, 100, 0.05)}
            </div>
            ${rangeInput('旋转', 'rotation', Math.round(THREE.MathUtils.radToDeg(asset.rotation)), -180, 180, 1, '°')}
            ${rangeInput('透明度', 'opacity', Math.round(asset.opacity * 100), 0, 100, 1, '%')}
            <div class="editor-actions">
              <button class="editor-small-button" type="button" data-action="depth-back">放到后面</button>
              <button class="editor-small-button" type="button" data-action="depth-front">放到前面</button>
            </div>
            ${physicsSelect(asset.physicsType)}
            ${asset.componentType === 'physics'
              ? numberInput('碎裂块数', 'fracturePieces', asset.fracturePieces, 4, 8, 1)
              : ''}
            ${animationSelect(asset.animation.type)}
            ${numberInput('转动速度', 'animation.speed', asset.animation.speed, -720, 720, 5)}
            <div class="editor-inline-fields">
              ${numberInput('碰撞宽度', 'collider.width', asset.collider.width, 0.1, 100, 0.05)}
              ${numberInput('碰撞高度', 'collider.height', asset.collider.height, 0.1, 100, 0.05)}
              ${numberInput('碰撞偏移 X', 'collider.offsetX', asset.collider.offsetX, -50, 50, 0.05)}
              ${numberInput('碰撞偏移 Y', 'collider.offsetY', asset.collider.offsetY, -50, 50, 0.05)}
            </div>
          </div>
        </details>
      </section>
    `;
  }

  async function getSourceTexture(url) {
    const cacheKey = url;
    let imagePromise = imageCache.get(cacheKey);
    if (!imagePromise) {
      imagePromise = loadImage(`${url}${url.includes('?') ? '&' : '?'}editor-cache=${Date.now()}`);
      imageCache.set(cacheKey, imagePromise);
    }
    const texture = new THREE.Texture(await imagePromise);
    texture.colorSpace = THREE.SRGBColorSpace;
    texture.magFilter = THREE.LinearFilter;
    texture.minFilter = THREE.LinearMipmapLinearFilter;
    texture.generateMipmaps = true;
    texture.needsUpdate = true;
    return texture;
  }

  function disposeSceneArt() {
    for (const mesh of [
      sceneArtRecord.pastBackdrop,
      sceneArtRecord.presentBackdrop,
      sceneArtRecord.pastMesh,
      sceneArtRecord.presentMesh,
    ]) {
      if (!mesh) continue;
      mesh.geometry?.dispose?.();
      mesh.material?.map?.dispose?.();
      mesh.material?.dispose?.();
      sceneArtRoot.remove(mesh);
    }
    sceneArtRecord = {
      pastBackdrop: null,
      presentBackdrop: null,
      pastMesh: null,
      presentMesh: null,
    };
  }

  async function rebuildSceneArt() {
    disposeSceneArt();
    const art = editorState.sceneArt;
    if (!art?.pastImage || !art?.presentImage) return;
    try {
      const [pastTexture, presentTexture] = await Promise.all([
        getSourceTexture(art.pastImage),
        getSourceTexture(art.presentImage),
      ]);
      const makeMesh = (texture, z) => {
        const material = new THREE.MeshBasicMaterial({
          map: texture,
          transparent: true,
          depthWrite: false,
        });
        const mesh = new THREE.Mesh(new THREE.PlaneGeometry(1, 1), material);
        mesh.position.set(art.x, art.y, z);
        mesh.scale.set(art.width, art.height, 1);
        mesh.renderOrder = 100;
        mesh.userData.editorLocked = true;
        return mesh;
      };
      const makeBackdrop = (color, z) => {
        const material = new THREE.MeshBasicMaterial({
          color,
          transparent: true,
          depthWrite: false,
        });
        const mesh = new THREE.Mesh(new THREE.PlaneGeometry(1, 1), material);
        mesh.position.set(2.5, 0, z);
        mesh.scale.set(48, 18, 1);
        mesh.renderOrder = 90;
        mesh.userData.editorLocked = true;
        return mesh;
      };
      sceneArtRecord.pastBackdrop = makeBackdrop('#2a1513', .84);
      sceneArtRecord.presentBackdrop = makeBackdrop('#071a20', .85);
      sceneArtRecord.pastMesh = makeMesh(pastTexture, .9);
      sceneArtRecord.presentMesh = makeMesh(presentTexture, .91);
      sceneArtRoot.add(
        sceneArtRecord.pastBackdrop,
        sceneArtRecord.presentBackdrop,
        sceneArtRecord.pastMesh,
        sceneArtRecord.presentMesh,
      );
    } catch (error) {
      showStatus(`整景美术加载失败：${error.message}`, true);
    }
  }

  function disposeGroup(group) {
    group.traverse(object => {
      object.geometry?.dispose?.();
      if (object.material) {
        const materials = Array.isArray(object.material) ? object.material : [object.material];
        for (const item of materials) {
          item.map?.dispose?.();
          item.dispose?.();
        }
      }
    });
    group.clear();
  }

  function syncAssetTransform(asset) {
    const record = assetGroups.get(asset.id);
    if (!record) return;
    record.group.position.set(asset.x, asset.y, 3 + asset.depth * 0.01);
    record.group.rotation.z = asset.rotation;
    record.group.scale.set(asset.width, asset.height, 1);
    record.group.visible = asset.visible !== false;
    record.group.renderOrder = 1000 + asset.depth;
    for (const mesh of [record.pastMesh, record.presentMesh]) {
      if (!mesh) continue;
      mesh.renderOrder = 1000 + asset.depth;
    }
  }

  async function rebuildAsset(asset) {
    let record = assetGroups.get(asset.id);
    if (!record) {
      const group = new THREE.Group();
      group.name = asset.id;
      assetRoot.add(group);
      record = {
        group,
        pastMesh: null,
        presentMesh: null,
        hitMesh: null,
        pastAlpha: null,
        presentAlpha: null,
      };
      assetGroups.set(asset.id, record);
    }

    const generation = (textureGeneration.get(asset.id) || 0) + 1;
    textureGeneration.set(asset.id, generation);
    disposeGroup(record.group);
    record.pastMesh = null;
    record.presentMesh = null;
    record.pastAlpha = null;
    record.presentAlpha = null;
    const hitMesh = new THREE.Mesh(
      new THREE.PlaneGeometry(1, 1),
      new THREE.MeshBasicMaterial({ transparent: true, opacity: 0, depthWrite: false }),
    );
    hitMesh.userData.editorAssetId = asset.id;
    record.group.add(hitMesh);
    record.hitMesh = hitMesh;
    syncAssetTransform(asset);
    if (!asset.versions.length) {
      refreshAssetMeshes();
      return;
    }

    const version = asset.versions[clamp(asset.activeVersion, 0, asset.versions.length - 1)];
    try {
      const [pastTexture, presentTexture] = await Promise.all([
        getSourceTexture(version.pastImage),
        getSourceTexture(version.presentImage),
      ]);
      if (textureGeneration.get(asset.id) !== generation) {
        pastTexture.dispose();
        presentTexture.dispose();
        return;
      }
      const geometry = new THREE.PlaneGeometry(1, 1);
      const pastMaterial = new THREE.MeshBasicMaterial({
        map: pastTexture,
        transparent: true,
        depthWrite: false,
        alphaTest: 0.01,
      });
      const presentMaterial = new THREE.MeshBasicMaterial({
        map: presentTexture,
        transparent: true,
        depthWrite: false,
        alphaTest: 0.01,
      });
      const pastMesh = new THREE.Mesh(geometry, pastMaterial);
      const presentMesh = new THREE.Mesh(geometry.clone(), presentMaterial);
      pastMesh.userData.editorAssetId = asset.id;
      presentMesh.userData.editorAssetId = asset.id;
      record.group.add(pastMesh, presentMesh);
      record.pastMesh = pastMesh;
      record.presentMesh = presentMesh;
      record.pastAlpha = createAlphaMask(pastTexture.image);
      record.presentAlpha = createAlphaMask(presentTexture.image);
      syncAssetTransform(asset);
      refreshAssetMeshes();
    } catch (error) {
      showStatus(`资产 ${asset.name} 加载失败：${error.message}`, true);
    }
  }

  function rebuildAllAssets() {
    const ids = new Set(editorState.assets.map(asset => asset.id));
    for (const [id, record] of assetGroups) {
      if (ids.has(id)) continue;
      disposeGroup(record.group);
      assetRoot.remove(record.group);
      assetGroups.delete(id);
    }
    for (const asset of editorState.assets) rebuildAsset(asset);
    refreshAssetMeshes();
  }

  function refreshAssetMeshes() {
    assetMeshes.length = 0;
    for (const record of assetGroups.values()) {
      if (record.hitMesh) assetMeshes.push(record.hitMesh);
    }
  }

  function createAlphaMask(image) {
    const imageWidth = image.naturalWidth || image.width;
    const imageHeight = image.naturalHeight || image.height;
    const scale = Math.min(1, 320 / Math.max(imageWidth, imageHeight));
    const width = Math.max(1, Math.round(imageWidth * scale));
    const height = Math.max(1, Math.round(imageHeight * scale));
    const maskCanvas = document.createElement('canvas');
    maskCanvas.width = width;
    maskCanvas.height = height;
    const context = maskCanvas.getContext('2d', { willReadFrequently: true });
    context.drawImage(image, 0, 0, width, height);
    return { width, height, pixels: context.getImageData(0, 0, width, height).data };
  }

  function sampleAlpha(mask, uv) {
    if (!mask || !uv) return 255;
    const x = clamp(Math.floor(uv.x * mask.width), 0, mask.width - 1);
    const y = clamp(Math.floor((1 - uv.y) * mask.height), 0, mask.height - 1);
    return mask.pixels[(y * mask.width + x) * 4 + 3];
  }

  function fracturePerimeterPoint(index, count) {
    const distance = (index / count) * 4;
    if (distance < 1) return { x: -.5 + distance, y: .5 };
    if (distance < 2) return { x: .5, y: .5 - (distance - 1) };
    if (distance < 3) return { x: .5 - (distance - 2), y: -.5 };
    return { x: -.5, y: -.5 + (distance - 3) };
  }

  function createFractureGeometry(asset, points, center) {
    const positions = [];
    const uvs = [];
    for (const point of points) {
      positions.push((point.x - center.x) * asset.width, (point.y - center.y) * asset.height, 0);
      uvs.push(point.x + .5, point.y + .5);
    }
    const geometry = new THREE.BufferGeometry();
    geometry.setAttribute('position', new THREE.Float32BufferAttribute(positions, 3));
    geometry.setAttribute('uv', new THREE.Float32BufferAttribute(uvs, 2));
    geometry.computeBoundingSphere();
    return geometry;
  }

  function createFractureMaterial(texture, opacity) {
    return new THREE.MeshBasicMaterial({
      map: texture,
      transparent: true,
      opacity,
      depthWrite: false,
      alphaTest: .01,
      side: THREE.DoubleSide,
    });
  }

  function clearFracture(assetId) {
    const fracture = fractureRecords.get(assetId);
    if (fracture) {
      for (const chunk of fracture.chunks) {
        chunk.group.traverse(object => {
          object.geometry?.dispose?.();
          object.material?.dispose?.();
        });
        assetRoot.remove(chunk.group);
      }
      fractureRecords.delete(assetId);
    }
    fracturedAssets.delete(assetId);
    const asset = findAsset(assetId);
    const record = assetGroups.get(assetId);
    if (record && asset) syncAssetTransform(asset);
  }

  function resetAllFractures() {
    for (const assetId of [...fracturedAssets]) clearFracture(assetId);
  }

  function fracturePhysicsAsset(asset, impulseX = 1, impulseY = 0) {
    if (!asset || fracturedAssets.has(asset.id) || inferComponentType(asset) !== 'physics') return false;
    const record = assetGroups.get(asset.id);
    const pastTexture = record?.pastMesh?.material?.map;
    const presentTexture = record?.presentMesh?.material?.map;
    if (!record || !pastTexture || !presentTexture) return false;

    const pieceCount = clamp(Math.round(Number(asset.fracturePieces) || 4), 4, 8);
    const era = clamp(getEra(), 0, 1);
    const impulseLength = Math.max(.001, Math.hypot(impulseX, impulseY));
    const directionX = impulseX / impulseLength;
    const directionY = impulseY / impulseLength;
    const tearCenter = { x: .035, y: -.025 };
    const chunks = [];

    for (let index = 0; index < pieceCount; index++) {
      const points = [
        tearCenter,
        fracturePerimeterPoint(index, pieceCount),
        fracturePerimeterPoint(index + 1, pieceCount),
      ];
      const center = {
        x: points.reduce((sum, point) => sum + point.x, 0) / points.length,
        y: points.reduce((sum, point) => sum + point.y, 0) / points.length,
      };
      const geometry = createFractureGeometry(asset, points, center);
      const pastMesh = new THREE.Mesh(geometry, createFractureMaterial(pastTexture, asset.opacity * (1 - era)));
      const presentMesh = new THREE.Mesh(geometry.clone(), createFractureMaterial(presentTexture, asset.opacity * era));
      const group = new THREE.Group();
      const localX = center.x * asset.width;
      const localY = center.y * asset.height;
      const cosine = Math.cos(asset.rotation);
      const sine = Math.sin(asset.rotation);
      group.position.set(
        asset.x + localX * cosine - localY * sine,
        asset.y + localX * sine + localY * cosine,
        6 + asset.depth * .01,
      );
      group.rotation.z = asset.rotation;
      group.name = `${asset.id}-fracture-${index + 1}`;
      group.add(pastMesh, presentMesh);
      pastMesh.renderOrder = 5000 + asset.depth;
      presentMesh.renderOrder = 5000 + asset.depth;
      assetRoot.add(group);

      const outwardLength = Math.max(.001, Math.hypot(center.x, center.y));
      const outwardX = center.x / outwardLength;
      const outwardY = center.y / outwardLength;
      const localVertices = points.map(point => ({
        x: (point.x - center.x) * asset.width,
        y: (point.y - center.y) * asset.height,
      }));
      chunks.push({
        group,
        pastMesh,
        presentMesh,
        localVertices,
        vx: directionX * 3.8 + outwardX * (2.3 + index * .18),
        vy: Math.max(1.8, directionY * 2.2) + 3.2 + Math.max(0, outwardY) * 1.8,
        angularVelocity: (index % 2 ? 1 : -1) * (2.1 + index * .48),
        sleeping: false,
      });
    }

    fracturedAssets.add(asset.id);
    fractureRecords.set(asset.id, { asset, chunks, elapsed: 0 });
    record.group.visible = false;
    return true;
  }

  function assetLocalPoint(asset, x, y) {
    const dx = x - asset.x;
    const dy = y - asset.y;
    const cosine = Math.cos(asset.rotation);
    const sine = Math.sin(asset.rotation);
    return { x: dx * cosine + dy * sine, y: -dx * sine + dy * cosine };
  }

  function segmentIntersectsAsset(asset, x1, y1, x2, y2, padding = 0) {
    const start = assetLocalPoint(asset, x1, y1);
    const end = assetLocalPoint(asset, x2, y2);
    const halfW = Math.abs(asset.width) * .5 + padding;
    const halfH = Math.abs(asset.height) * .5 + padding;
    const dx = end.x - start.x;
    const dy = end.y - start.y;
    let minimum = 0;
    let maximum = 1;
    for (const [origin, delta, low, high] of [
      [start.x, dx, -halfW, halfW],
      [start.y, dy, -halfH, halfH],
    ]) {
      if (Math.abs(delta) < 1e-7) {
        if (origin < low || origin > high) return false;
        continue;
      }
      const first = (low - origin) / delta;
      const second = (high - origin) / delta;
      minimum = Math.max(minimum, Math.min(first, second));
      maximum = Math.min(maximum, Math.max(first, second));
      if (minimum > maximum) return false;
    }
    return true;
  }

  function fracturePhysicsAlongSegment(x1, y1, x2, y2, padding = .08, impulseX = x2 - x1, impulseY = y2 - y1) {
    const candidates = editorState.assets
      .filter(asset => asset.visible !== false
        && asset.versions.length
        && inferComponentType(asset) === 'physics'
        && !fracturedAssets.has(asset.id))
      .sort((a, b) => b.depth - a.depth);
    const asset = candidates.find(item => segmentIntersectsAsset(item, x1, y1, x2, y2, padding));
    return asset ? fracturePhysicsAsset(asset, impulseX, impulseY) : false;
  }

  function lowestChunkPoint(chunk) {
    const cosine = Math.cos(chunk.group.rotation.z);
    const sine = Math.sin(chunk.group.rotation.z);
    return Math.min(...chunk.localVertices.map(vertex => (
      chunk.group.position.y + vertex.x * sine + vertex.y * cosine
    )));
  }

  function updateFracturePhysics(dt) {
    for (const fracture of fractureRecords.values()) {
      fracture.elapsed += dt;
      for (const chunk of fracture.chunks) {
        if (chunk.sleeping) continue;
        chunk.vy -= 18 * dt;
        chunk.group.position.x += chunk.vx * dt;
        chunk.group.position.y += chunk.vy * dt;
        chunk.group.rotation.z += chunk.angularVelocity * dt;
        const ground = getGroundForY(chunk.group.position.y);
        const lowest = lowestChunkPoint(chunk);
        if (lowest < ground) {
          chunk.group.position.y += ground - lowest;
          if (Math.abs(chunk.vy) > .72) chunk.vy = -chunk.vy * .24;
          else chunk.vy = 0;
          chunk.vx *= .72;
          chunk.angularVelocity *= .68;
          if (Math.abs(chunk.vx) < .08 && Math.abs(chunk.angularVelocity) < .08) chunk.sleeping = true;
        }
        if (fracture.elapsed > 8) chunk.sleeping = true;
      }
    }
  }

  function updateFractureAppearance(era) {
    for (const fracture of fractureRecords.values()) {
      for (const chunk of fracture.chunks) {
        chunk.pastMesh.material.opacity = fracture.asset.opacity * (1 - era);
        chunk.pastMesh.visible = chunk.pastMesh.material.opacity > .005;
        chunk.presentMesh.material.opacity = fracture.asset.opacity * era;
        chunk.presentMesh.visible = chunk.presentMesh.material.opacity > .005;
      }
    }
  }

  function switchVersion(asset, direction) {
    if (!asset?.versions.length) return;
    asset.activeVersion = (asset.activeVersion + direction + asset.versions.length) % asset.versions.length;
    rebuildAsset(asset);
    saveStateSoon();
    renderInspector();
    updateSelectionOverlay();
  }

  async function captureSelection(bounds) {
    const rect = rectFromWorldBounds(bounds);
    const canvasRect = canvas.getBoundingClientRect();
    const scaleX = canvas.width / canvasRect.width;
    const scaleY = canvas.height / canvasRect.height;
    const sx = clamp(Math.round((rect.left - canvasRect.left) * scaleX), 0, canvas.width - 1);
    const sy = clamp(Math.round((rect.top - canvasRect.top) * scaleY), 0, canvas.height - 1);
    const sw = clamp(Math.round(rect.width * scaleX), 1, canvas.width - sx);
    const sh = clamp(Math.round(rect.height * scaleY), 1, canvas.height - sy);
    const hidden = [];
    scene.traverse(object => {
      if (object.userData.editorExcludeFromCapture && object.visible) {
        hidden.push(object);
        object.visible = false;
      }
    });
    const assetVisibility = assetRoot.visible;
    assetRoot.visible = false;
    renderer.render(scene, camera);
    const output = document.createElement('canvas');
    output.width = clamp(sw, 64, 2048);
    output.height = clamp(sh, 64, 2048);
    const context = output.getContext('2d');
    context.drawImage(canvas, sx, sy, sw, sh, 0, 0, output.width, output.height);
    assetRoot.visible = assetVisibility;
    for (const object of hidden) object.visible = true;
    renderer.render(scene, camera);
    return output.toDataURL('image/png');
  }

  function generationBrief(asset) {
    const addingNewComponent = asset.generationIntent === 'add';
    return [
      addingNewComponent
        ? `在框选区域中新增“${asset.description}”，输出可直接用于游戏的高质量正交侧视 2D PNG 独立组件；不得生成框外内容。`
        : `只制作框选区域中的“${asset.description}”，输出可直接用于游戏的高质量正交侧视 2D PNG 独立组件。`,
      '必须输出同一物体的 2047 完整版与 2147 破败版，两张图轮廓、画布尺寸、透明留白、朝向和锚点完全一致。',
      addingNewComponent
        ? '使用框选截图锁定可用空间、落点、比例和邻接关系；必须严格匹配时代参考图的造型语言、材质、调色板、边缘处理与细节密度，不能出现另一套画风。'
        : '使用框选截图锁定原位置、轮廓和比例，使用时代效果图决定造型语言、材质、调色板与细节密度。',
      '全部采用 Unlit 表现：透明背景、无环境、无地面、无投影、无体积光、无方向性光照，不在图片内烘焙场景光。',
      '直接保留图像生成结果的原始 PNG 画质；禁止像素化、调色板量化、抖动、额外描边或噪点后处理。',
      '2047 版本完整且仍在使用；2147 版本是同一物体自然老化、损坏和缺失零件后的结果，不改变物体身份。',
      `组件类别：${COMPONENT_TYPE_LABELS[asset.componentType] || COMPONENT_TYPE_LABELS.static}；物理行为：${PHYSICS_LABELS[asset.physicsType] || PHYSICS_LABELS['static-visual']}。`,
      asset.animation?.type === 'rotate'
        ? `该组件会以 ${asset.animation.speed}°/秒绕画布中心持续旋转，必须保证轴心精确居中且旋转轮廓稳定。`
        : '该组件没有固定动画。',
    ].join('\n');
  }

  function inferDraftBehavior(source) {
    const description = String(source?.description || '').toLowerCase();
    const physical = /(可以?推|能推|推开|可推动|物理|碰撞|可破坏|能打碎|会碎|碎裂|木箱|箱子|矿车|油桶)/i.test(description);
    const animated = /(旋转|转动|会转|一直转|持续转|循环运动|动画|风扇|齿轮|传送带)/i.test(description);
    if (physical) applyComponentType(source, 'physics');
    else if (animated) applyComponentType(source, 'animation');
    else applyComponentType(source, 'static');
  }

  async function copyTextToClipboard(text) {
    try {
      await navigator.clipboard.writeText(text);
      return true;
    } catch {
      const fallback = document.createElement('textarea');
      fallback.value = text;
      fallback.setAttribute('readonly', '');
      fallback.style.position = 'fixed';
      fallback.style.opacity = '0';
      document.body.append(fallback);
      fallback.select();
      const copied = document.execCommand('copy');
      fallback.remove();
      if (!copied) throw new Error('浏览器没有允许复制，请再点一次“复制给 Codex”');
      return true;
    }
  }

  async function createGenerationJob(source = draft, existingAsset = null) {
    if (!source?.description?.trim()) {
      showStatus('先说一句这个框里想放什么', true);
      return;
    }
    if (!editorState.references.past.length || !editorState.references.present.length) {
      showStatus('两个时代都至少需要一张参考图', true);
      return;
    }

    const bounds = existingAsset
      ? {
        minX: existingAsset.x - existingAsset.width * 0.5,
        maxX: existingAsset.x + existingAsset.width * 0.5,
        minY: existingAsset.y - existingAsset.height * 0.5,
        maxY: existingAsset.y + existingAsset.height * 0.5,
      }
      : source.bounds;
    const asset = existingAsset || createDefaultAsset(bounds, source.description, source.sourceIds);
    if (!existingAsset) {
      if (source.intent === 'add') inferDraftBehavior(source);
      asset.physicsType = source.physicsType;
      asset.animation = clone(source.animation || DEFAULT_ANIMATION);
      asset.componentType = source.componentType || 'static';
      asset.generationIntent = source.intent === 'add' ? 'add' : 'replace';
      asset.sourceComponentId = source.selectedComponentId && source.selectedComponentId !== 'custom'
        ? source.selectedComponentId
        : null;
      editorState.assets.push(asset);
      renderComponentMap();
    }
    const timestamp = Date.now();
    const versionId = `v-${timestamp}`;
    const jobId = `job-${timestamp}-${Math.random().toString(36).slice(2, 7)}`;
    asset.pendingJobId = jobId;
    const selectionImage = await captureSelection(bounds);
    const references = {
      past: editorState.references.past.map(reference => ({ ...reference, file: `public${reference.url}` })),
      present: editorState.references.present.map(reference => ({ ...reference, file: `public${reference.url}` })),
    };
    const response = await fetch('/api/editor/job', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({
        jobId,
        assetId: asset.id,
        versionId,
        description: asset.description,
        selectionImage,
        selectionWorldBounds: bounds,
        renderMode: 'source-png',
        intent: asset.generationIntent || 'replace',
        componentType: asset.componentType,
        animation: asset.animation,
        references,
        generationBrief: generationBrief(asset),
      }),
    });
    const result = await response.json();
    if (!response.ok) {
      asset.pendingJobId = null;
      if (!existingAsset) editorState.assets = editorState.assets.filter(item => item.id !== asset.id);
      throw new Error(result.error || '创建 Codex 任务失败');
    }
    await saveStateNow();
    draft = null;
    selectedAssetId = asset.id;
    setTool('select');
    renderInspector();
    updateSelectionOverlay();
    try {
      await copyTextToClipboard(result.command);
      showStatus('提示词已复制。现在到 Codex 里粘贴发送即可');
    } catch (error) {
      showStatus(error.message || `任务已创建：${result.requestFile}`, true);
    }
    startPolling();
  }

  async function pollPendingJobs() {
    const pending = editorState.assets.filter(asset => asset.pendingJobId);
    for (const asset of pending) {
      try {
        const response = await fetch(`/api/editor/job-status?id=${encodeURIComponent(asset.pendingJobId)}`, { cache: 'no-store' });
        if (!response.ok) continue;
        const result = await response.json();
        if (result.status === 'complete') {
          asset.versions.push({
            id: result.versionId || `v-${Date.now()}`,
            pastImage: result.pastImage,
            presentImage: result.presentImage,
            completedAt: result.completedAt || new Date().toISOString(),
            promptSummary: result.promptSummary || '',
          });
          asset.activeVersion = asset.versions.length - 1;
          asset.pendingJobId = null;
          imageCache.delete(result.pastImage);
          imageCache.delete(result.presentImage);
          await rebuildAsset(asset);
          renderComponentMap();
          refreshWhiteboxVisibility();
          await saveStateNow();
          renderInspector();
          showStatus(`${asset.name} 的双时代资产已自动采用`);
        } else if (result.status === 'failed') {
          asset.pendingJobId = null;
          await saveStateNow();
          renderInspector();
          showStatus(result.error || 'Codex 生成任务失败', true);
        }
      } catch {
        // A temporary read error should not discard a pending job.
      }
    }
  }

  function startPolling() {
    clearInterval(pollTimer);
    pollPendingJobs();
    pollTimer = setInterval(pollPendingJobs, 1800);
  }

  function setTool(nextTool) {
    tool = nextTool;
    elements.shell.querySelectorAll('[data-tool]').forEach(button => {
      button.classList.toggle('active', button.dataset.tool === tool);
    });
    document.body.classList.toggle('editor-select-tool', tool === 'select');
    elements.selectionRect.dataset.label = tool === 'box-add' ? '新物体放在这里' : '选择框内物体';
    elements.modeGuide.textContent = tool === 'box-add'
      ? '在场景中按住鼠标拖一个框'
      : (tool === 'box-select' ? '拖一个框，选择里面已有的物体' : '直接点击并拖动物体');
    if (tool !== 'select') selectedAssetId = null;
    draft = null;
    renderInspector();
    updateSelectionOverlay();
  }

  function setActive(nextActive) {
    active = nextActive;
    elements.shell.classList.toggle('open', active);
    elements.componentLayer.classList.toggle('open', active);
    elements.entry.setAttribute('aria-pressed', String(active));
    document.body.classList.toggle('editor-mode', active);
    document.body.classList.toggle('editor-select-tool', active && tool === 'select');
    elements.modeGuide.classList.toggle('show', active);
    if (active) {
      resetAllFractures();
      renderReferences();
      renderInspector();
      renderComponentMap();
      setTool(tool);
      showStatus('编辑模式：游戏已暂停');
    } else {
      boxDrag = null;
      assetDrag = null;
      draft = null;
      selectedAssetId = null;
      elements.selectionRect.style.display = 'none';
      elements.assetBox.style.display = 'none';
      elements.versionSwitcher.style.display = 'none';
      elements.regionComposer.classList.remove('show');
      elements.referenceBoard.classList.remove('open');
      document.body.classList.remove('editor-dragging');
    }
  }

  async function saveAndPlay() {
    try {
      await saveStateNow();
      setActive(false);
    } catch (error) {
      showStatus(error.message, true);
    }
  }

  function updateSelectionOverlay() {
    if (!active) return;
    elements.regionComposer.classList.remove('show');
    if (boxDrag) {
      const rect = {
        left: Math.min(boxDrag.startX, boxDrag.currentX),
        top: Math.min(boxDrag.startY, boxDrag.currentY),
        width: Math.abs(boxDrag.currentX - boxDrag.startX),
        height: Math.abs(boxDrag.currentY - boxDrag.startY),
      };
      rect.right = rect.left + rect.width;
      rect.bottom = rect.top + rect.height;
      setRectElement(elements.selectionRect, rect);
      elements.assetBox.style.display = 'none';
      elements.versionSwitcher.style.display = 'none';
      return;
    }
    if (draft) {
      const rect = rectFromWorldBounds(draft.bounds);
      setRectElement(elements.selectionRect, rect);
      elements.assetBox.style.display = 'none';
      elements.versionSwitcher.style.display = 'none';
      if (draft.intent === 'add') {
        const composerWidth = Math.min(390, innerWidth - 24);
        const composerHeight = 190;
        const left = clamp(rect.left, 12, innerWidth - composerWidth - 12);
        const below = rect.bottom + 10;
        const top = below + composerHeight <= innerHeight - 12
          ? below
          : Math.max(62, rect.top - composerHeight - 10);
        elements.regionComposer.style.width = `${composerWidth}px`;
        elements.regionComposer.style.left = `${left}px`;
        elements.regionComposer.style.top = `${top}px`;
        if (document.activeElement !== elements.regionPrompt
          && elements.regionPrompt.value !== draft.description) elements.regionPrompt.value = draft.description;
        elements.regionComposer.classList.add('show');
      }
      return;
    }
    elements.selectionRect.style.display = 'none';
    const asset = findAsset();
    if (!asset) {
      elements.assetBox.style.display = 'none';
      elements.versionSwitcher.style.display = 'none';
      return;
    }
    const cosine = Math.cos(asset.rotation);
    const sine = Math.sin(asset.rotation);
    const halfW = asset.width * 0.5;
    const halfH = asset.height * 0.5;
    const corners = [
      [-halfW, -halfH], [halfW, -halfH], [halfW, halfH], [-halfW, halfH],
    ].map(([x, y]) => clientFromWorld(
      asset.x + x * cosine - y * sine,
      asset.y + x * sine + y * cosine,
    ));
    const xs = corners.map(point => point.x);
    const ys = corners.map(point => point.y);
    const rect = {
      left: Math.min(...xs),
      right: Math.max(...xs),
      top: Math.min(...ys),
      bottom: Math.max(...ys),
    };
    rect.width = rect.right - rect.left;
    rect.height = rect.bottom - rect.top;
    setRectElement(elements.assetBox, rect);
    elements.versionSwitcher.style.display = 'flex';
    elements.versionSwitcher.style.left = `${clamp((rect.left + rect.right) * 0.5 - 62, 8, innerWidth - 132)}px`;
    elements.versionSwitcher.style.top = `${Math.max(58, rect.top - 39)}px`;
    elements.versionSwitcher.querySelector('span').textContent = asset.pendingJobId
      ? '生成中…'
      : `${asset.versions.length ? asset.activeVersion + 1 : 0} / ${asset.versions.length}`;
  }

  function createDraftFromScreenRect(screenRect, selectionMode = tool) {
    const topLeft = worldFromClient(screenRect.left, screenRect.top);
    const bottomRight = worldFromClient(screenRect.right, screenRect.bottom);
    const bounds = {
      minX: Math.min(topLeft.x, bottomRight.x),
      maxX: Math.max(topLeft.x, bottomRight.x),
      minY: Math.min(topLeft.y, bottomRight.y),
      maxY: Math.max(topLeft.y, bottomRight.y),
    };
    const addingNewComponent = selectionMode === 'box-add';
    const candidates = addingNewComponent ? [] : componentsInside(bounds);
    if (!addingNewComponent && !candidates.length) {
      showStatus('框内没有已有组件；要创建新组件请使用“框选新增 · F”', true);
      updateSelectionOverlay();
      return;
    }
    draft = {
      bounds,
      width: bounds.maxX - bounds.minX,
      height: bounds.maxY - bounds.minY,
      description: '',
      intent: addingNewComponent ? 'add' : 'select-existing',
      componentType: 'static',
      physicsType: 'static-visual',
      animation: clone(DEFAULT_ANIMATION),
      candidates,
      selectedComponentId: addingNewComponent ? 'custom' : null,
      sourceIds: {
        past: collectWhiteboxIds(pastLayer, 'past', screenRect),
        present: collectWhiteboxIds(presentLayer, 'present', screenRect),
      },
    };
    renderInspector();
    updateSelectionOverlay();
    if (addingNewComponent) requestAnimationFrame(() => elements.regionPrompt.focus());
  }

  function pointerToNdc(event) {
    const rect = canvas.getBoundingClientRect();
    pointerNdc.set(
      ((event.clientX - rect.left) / rect.width) * 2 - 1,
      -((event.clientY - rect.top) / rect.height) * 2 + 1,
    );
  }

  function hitAsset(event) {
    pointerToNdc(event);
    raycaster.setFromCamera(pointerNdc, camera);
    const hits = raycaster.intersectObjects(assetMeshes, false);
    const era = clamp(getEra(), 0, 1);
    for (const hit of hits) {
      const assetId = hit.object.userData.editorAssetId;
      const record = assetGroups.get(assetId);
      if (!record) continue;
      const alpha = sampleAlpha(record.pastAlpha, hit.uv) * (1 - era)
        + sampleAlpha(record.presentAlpha, hit.uv) * era;
      if (alpha > 24) return assetId;
    }
    return null;
  }

  function handleCanvasPointerDown(event) {
    if (!active || event.button !== 0) return;
    const assetId = tool === 'select' ? hitAsset(event) : null;
    if (assetId) {
      selectedAssetId = assetId;
      draft = null;
      const asset = findAsset(assetId);
      const world = worldFromClient(event.clientX, event.clientY);
      assetDrag = { assetId, offsetX: asset.x - world.x, offsetY: asset.y - world.y };
      document.body.classList.add('editor-dragging');
      canvas.setPointerCapture?.(event.pointerId);
      renderInspector();
      updateSelectionOverlay();
      updateComponentMap();
      event.preventDefault();
      return;
    }
    if (tool === 'box-select' || tool === 'box-add') {
      boxDrag = {
        startX: event.clientX,
        startY: event.clientY,
        currentX: event.clientX,
        currentY: event.clientY,
      };
      draft = null;
      selectedAssetId = null;
      canvas.setPointerCapture?.(event.pointerId);
      updateSelectionOverlay();
      event.preventDefault();
      return;
    }
    selectedAssetId = null;
    draft = null;
    renderInspector();
    updateSelectionOverlay();
    event.preventDefault();
  }

  function handleCanvasPointerMove(event) {
    if (!active) return;
    if (boxDrag) {
      boxDrag.currentX = event.clientX;
      boxDrag.currentY = event.clientY;
      updateSelectionOverlay();
      return;
    }
    if (!assetDrag) return;
    const asset = findAsset(assetDrag.assetId);
    if (!asset) return;
    const world = worldFromClient(event.clientX, event.clientY);
    asset.x = snapValue(world.x + assetDrag.offsetX);
    asset.y = snapValue(world.y + assetDrag.offsetY);
    syncAssetTransform(asset);
    updateSelectionOverlay();
  }

  function handleCanvasPointerUp(event) {
    if (!active || (event.type !== 'pointercancel' && event.button !== 0)) return;
    if (boxDrag) {
      const rect = {
        left: Math.min(boxDrag.startX, boxDrag.currentX),
        top: Math.min(boxDrag.startY, boxDrag.currentY),
        right: Math.max(boxDrag.startX, boxDrag.currentX),
        bottom: Math.max(boxDrag.startY, boxDrag.currentY),
      };
      rect.width = rect.right - rect.left;
      rect.height = rect.bottom - rect.top;
      boxDrag = null;
      if (rect.width >= 12 && rect.height >= 12) createDraftFromScreenRect(rect, tool);
      else updateSelectionOverlay();
    }
    if (assetDrag) {
      assetDrag = null;
      document.body.classList.remove('editor-dragging');
      saveStateSoon();
      renderInspector();
    }
    canvas.releasePointerCapture?.(event.pointerId);
  }

  function createAssetCopy(source, nameSuffix = '副本', offsetSteps = 1) {
    if (!source) return null;
    const copy = normalizeAsset(clone(source));
    copy.id = `asset-${Date.now()}-${Math.random().toString(36).slice(2, 7)}`;
    copy.name = `${source.name} ${nameSuffix}`;
    const offset = Math.max(0.05, editorState.editor.gridSize) * offsetSteps;
    copy.x = snapValue(source.x + offset);
    copy.y = snapValue(source.y + offset);
    copy.sourceIds = { past: [], present: [] };
    copy.sourceComponentId = null;
    copy.pendingJobId = null;
    copy.createdAt = new Date().toISOString();
    editorState.assets.push(copy);
    selectedAssetId = copy.id;
    rebuildAsset(copy);
    renderComponentMap();
    saveStateSoon();
    renderInspector();
    updateSelectionOverlay();
    return copy;
  }

  function duplicateSelectedAsset() {
    const copy = createAssetCopy(findAsset());
    if (copy) showStatus(`已复制 ${copy.name}`);
  }

  function copySelectedAsset() {
    const asset = findAsset();
    if (!asset) {
      showStatus('请先选择一个组件再复制', true);
      return;
    }
    copiedAsset = clone(asset);
    pasteCount = 0;
    showStatus(`已复制 ${asset.name}；按 Ctrl+V 粘贴`);
  }

  function pasteCopiedAsset() {
    if (!copiedAsset) {
      showStatus('编辑器剪贴板为空；请先按 Ctrl+C', true);
      return;
    }
    pasteCount += 1;
    const copy = createAssetCopy(copiedAsset, '复制', pasteCount);
    if (copy) showStatus(`已粘贴 ${copy.name}`);
  }

  function deleteSelectedAsset() {
    const asset = findAsset();
    if (!asset) return;
    const deletedName = asset.name;
    clearFracture(asset.id);
    editorState.assets = editorState.assets.filter(item => item.id !== asset.id);
    const record = assetGroups.get(asset.id);
    if (record) {
      disposeGroup(record.group);
      assetRoot.remove(record.group);
      assetGroups.delete(asset.id);
    }
    selectedAssetId = null;
    refreshAssetMeshes();
    renderComponentMap();
    refreshWhiteboxVisibility();
    saveStateSoon();
    renderInspector();
    updateSelectionOverlay();
    showStatus(`已删除 ${deletedName}`);
  }

  function handleInspectorInput(event) {
    const draftField = event.target.dataset.draftField;
    if (draftField && draft) {
      setNested(draft, draftField, event.target.value);
      return;
    }
    const field = event.target.dataset.field;
    if (!field) return;
    const target = draft || findAsset();
    if (!target) return;
    let value = event.target.value;
    if (event.target.type === 'checkbox') value = event.target.checked;
    else if (event.target.type === 'range' || event.target.type === 'number') value = Number(value);
    if (field === 'rotation' && !draft) value = THREE.MathUtils.degToRad(value);
    if (field === 'opacity' && !draft) value /= 100;
    if (field === 'componentType') applyComponentType(target, value);
    else {
      setNested(target, field, value);
      if (field === 'physicsType' && value !== 'static-visual') {
        target.componentType = 'physics';
        target.animation = { ...DEFAULT_ANIMATION, ...(target.animation || {}), type: 'none' };
      } else if (field === 'animation.type' && value !== 'none') {
        target.componentType = 'animation';
        target.physicsType = 'static-visual';
      } else if ((field === 'physicsType' || field === 'animation.type')
        && target.physicsType === 'static-visual'
        && target.animation?.type === 'none') {
        target.componentType = 'static';
      }
    }
    const output = event.target.nextElementSibling;
    if (output?.tagName === 'OUTPUT') {
      const suffix = field === 'rotation' ? '°'
        : (field === 'opacity' ? '%' : '');
      output.textContent = `${event.target.value}${suffix}`;
    }
    const behaviorChanged = field === 'componentType' || field === 'physicsType' || field === 'animation.type';
    if (draft) {
      if (behaviorChanged) renderInspector();
      updateComponentMap();
      return;
    }
    const asset = target;
    syncAssetTransform(asset);
    updateSelectionOverlay();
    updateComponentMap();
    saveStateSoon();
    if (behaviorChanged) renderInspector();
  }

  function chooseComponent(componentId) {
    const component = componentCatalog().find(item => item.id === componentId);
    if (!component) return;
    if (component.kind === 'asset') {
      draft = null;
      selectedAssetId = component.id;
      setTool('select');
      showStatus(`已选择 ${component.name}`);
      return;
    }
    const screenRect = rectFromWorldBounds(component.bounds);
    draft = {
      ...draft,
      bounds: clone(component.bounds),
      width: component.bounds.maxX - component.bounds.minX,
      height: component.bounds.maxY - component.bounds.minY,
      description: component.name,
      componentType: component.componentType,
      selectedComponentId: component.id,
      candidates: [],
      sourceIds: {
        past: collectWhiteboxIds(pastLayer, 'past', screenRect),
        present: collectWhiteboxIds(presentLayer, 'present', screenRect),
      },
    };
    applyComponentType(draft, component.componentType);
    renderInspector();
    updateSelectionOverlay();
    updateComponentMap();
  }

  async function handleInspectorAction(action, data = {}) {
    const asset = findAsset();
    if (action === 'choose-component') {
      chooseComponent(data.componentId);
    } else if (action === 'use-drawn-region' && draft) {
      draft.selectedComponentId = 'custom';
      draft.candidates = [];
      renderInspector();
      updateComponentMap();
    } else if (action === 'create-job') {
      try {
        await createGenerationJob();
      } catch (error) {
        showStatus(error.message, true);
      }
    } else if (action === 'cancel-draft') {
      draft = null;
      renderInspector();
      updateSelectionOverlay();
    } else if (action === 'depth-back' && asset) {
      asset.depth -= 1;
      syncAssetTransform(asset);
      saveStateSoon();
      renderInspector();
    } else if (action === 'depth-front' && asset) {
      asset.depth += 1;
      syncAssetTransform(asset);
      saveStateSoon();
      renderInspector();
    } else if (action === 'version-previous') switchVersion(asset, -1);
    else if (action === 'version-next') switchVersion(asset, 1);
    else if (action === 'regenerate' && asset && !asset.pendingJobId) {
      try {
        await createGenerationJob(asset, asset);
        renderInspector();
      } catch (error) {
        showStatus(error.message, true);
      }
    } else if (action === 'duplicate') duplicateSelectedAsset();
    else if (action === 'delete') deleteSelectedAsset();
  }

  elements.entry.addEventListener('click', () => setActive(true));
  elements.savePlay.addEventListener('click', saveAndPlay);
  elements.referenceToggle.addEventListener('click', () => elements.referenceBoard.classList.toggle('open'));
  elements.referenceClose.addEventListener('click', () => elements.referenceBoard.classList.remove('open'));
  elements.regionPrompt.addEventListener('input', () => {
    if (draft?.intent === 'add') draft.description = elements.regionPrompt.value;
  });
  elements.regionPrompt.addEventListener('keydown', event => {
    if ((event.ctrlKey || event.metaKey) && event.key === 'Enter') {
      event.preventDefault();
      elements.regionComposer.requestSubmit();
    }
  });
  elements.regionComposer.addEventListener('submit', async event => {
    event.preventDefault();
    if (draft?.intent !== 'add') return;
    draft.description = elements.regionPrompt.value;
    const submit = elements.regionComposer.querySelector('[type="submit"]');
    submit.disabled = true;
    submit.textContent = '正在准备…';
    try {
      await createGenerationJob();
    } catch (error) {
      showStatus(error.message, true);
    } finally {
      submit.disabled = false;
      submit.textContent = '复制给 Codex';
    }
  });
  elements.regionComposer.querySelector('[data-region-cancel]').addEventListener('click', () => {
    draft = null;
    renderInspector();
    updateSelectionOverlay();
  });
  elements.shell.addEventListener('click', event => {
    const toolButton = event.target.closest('[data-tool]');
    if (toolButton) setTool(toolButton.dataset.tool);
    const dropButton = event.target.closest('[data-reference-drop]');
    if (dropButton) buildReferenceFileInput(dropButton.dataset.referenceDrop);
    const removeButton = event.target.closest('[data-remove-reference]');
    if (removeButton) {
      const slot = removeButton.dataset.slot;
      editorState.references[slot] = editorState.references[slot].filter(reference => reference.id !== removeButton.dataset.removeReference);
      renderReferences();
      saveStateSoon();
    }
  });
  elements.shell.addEventListener('dragover', event => {
    const drop = event.target.closest('[data-reference-drop]');
    if (!drop) return;
    event.preventDefault();
    drop.classList.add('dragging');
  });
  elements.shell.addEventListener('dragleave', event => event.target.closest('[data-reference-drop]')?.classList.remove('dragging'));
  elements.shell.addEventListener('drop', event => {
    const drop = event.target.closest('[data-reference-drop]');
    if (!drop) return;
    event.preventDefault();
    drop.classList.remove('dragging');
    uploadReferences(drop.dataset.referenceDrop, event.dataTransfer.files).catch(error => showStatus(error.message, true));
  });
  elements.snap.addEventListener('change', () => {
    editorState.editor.snapEnabled = elements.snap.checked;
    saveStateSoon();
  });
  elements.gridSize.addEventListener('change', () => {
    editorState.editor.gridSize = clamp(Number(elements.gridSize.value) || 0.5, 0.05, 4);
    elements.gridSize.value = editorState.editor.gridSize;
    saveStateSoon();
  });
  elements.sceneArtToggle.addEventListener('change', () => {
    editorState.sceneArt.enabled = elements.sceneArtToggle.checked;
    sceneArtRoot.visible = editorState.sceneArt.enabled;
    saveStateSoon();
    showStatus(editorState.sceneArt.enabled ? '已显示完整场景美术' : '已暂时显示底层白盒');
  });
  elements.inspector.addEventListener('input', handleInspectorInput);
  elements.inspector.addEventListener('change', handleInspectorInput);
  elements.inspector.addEventListener('click', event => {
    const button = event.target.closest('[data-action]');
    if (button) handleInspectorAction(button.dataset.action, button.dataset);
  });
  elements.versionSwitcher.addEventListener('click', event => {
    const button = event.target.closest('[data-version]');
    if (!button) return;
    switchVersion(findAsset(), button.dataset.version === 'previous' ? -1 : 1);
  });
  canvas.addEventListener('pointerdown', handleCanvasPointerDown);
  canvas.addEventListener('pointermove', handleCanvasPointerMove);
  canvas.addEventListener('pointerup', handleCanvasPointerUp);
  canvas.addEventListener('pointercancel', handleCanvasPointerUp);

  function handleGameKeyDown(event) {
    if (!active) return false;
    const editingText = event.target instanceof HTMLInputElement
      || event.target instanceof HTMLTextAreaElement
      || event.target instanceof HTMLSelectElement
      || event.target?.isContentEditable;
    if (editingText && event.code !== 'Escape') {
      event.stopImmediatePropagation();
      return true;
    }
    if (event.code === 'Escape') {
      if (draft) {
        draft = null;
        renderInspector();
        updateSelectionOverlay();
      } else setActive(false);
    } else if ((event.ctrlKey || event.metaKey) && event.code === 'KeyC') copySelectedAsset();
    else if ((event.ctrlKey || event.metaKey) && event.code === 'KeyV') pasteCopiedAsset();
    else if ((event.ctrlKey || event.metaKey) && event.code === 'KeyD') duplicateSelectedAsset();
    else if (event.code === 'KeyF') setTool('box-add');
    else if (event.code === 'KeyB') setTool('box-select');
    else if (event.code === 'KeyV') setTool('select');
    else if (event.code === 'Delete' || event.code === 'Backspace') deleteSelectedAsset();
    event.preventDefault();
    event.stopImmediatePropagation();
    return true;
  }

  function currentColliders() {
    return editorState.assets
      .filter(asset => asset.visible !== false
        && asset.versions.length
        && asset.physicsType !== 'static-visual'
        && !fracturedAssets.has(asset.id))
      .map(asset => ({
        asset,
        centerX: asset.x + asset.collider.offsetX,
        centerY: asset.y + asset.collider.offsetY,
        halfW: Math.abs(asset.collider.width) * 0.5,
        halfH: Math.abs(asset.collider.height) * 0.5,
      }));
  }

  function resolveHorizontalPlayer(player, previousX, proposedX) {
    let nextX = proposedX;
    for (const collider of currentColliders()) {
      const verticalOverlap = Math.abs(player.y - collider.centerY) < player.halfH + collider.halfH - 0.03;
      if (!verticalOverlap || Math.abs(nextX - collider.centerX) >= player.halfW + collider.halfW) continue;
      const movingRight = nextX >= previousX;
      if (collider.asset.physicsType === 'pushable') {
        const desiredCenter = movingRight
          ? nextX + player.halfW + collider.halfW
          : nextX - player.halfW - collider.halfW;
        const movement = desiredCenter - collider.centerX;
        collider.asset.x += movement;
        syncAssetTransform(collider.asset);
        clearTimeout(physicsSaveTimer);
        physicsSaveTimer = setTimeout(saveStateSoon, 500);
        continue;
      }
      nextX = movingRight
        ? collider.centerX - collider.halfW - player.halfW
        : collider.centerX + collider.halfW + player.halfW;
      player.vx = 0;
    }
    return nextX;
  }

  function resolveLandingY(player, previousBottom, nextBottom, baseLandingY) {
    let landingY = baseLandingY;
    for (const collider of currentColliders()) {
      const top = collider.centerY + collider.halfH;
      const horizontalOverlap = Math.abs(player.x - collider.centerX) <= player.halfW + collider.halfW * 0.96;
      if (horizontalOverlap && previousBottom >= top - 0.09 && nextBottom <= top) landingY = Math.max(landingY, top);
    }
    return landingY;
  }

  function updateGamePhysics(dt) {
    for (const asset of editorState.assets) {
      if (asset.visible === false
        || asset.physicsType !== 'pushable'
        || !asset.versions.length
        || fracturedAssets.has(asset.id)) continue;
      const ground = getGroundForY(asset.y);
      const bottomOffset = asset.collider.offsetY - Math.abs(asset.collider.height) * 0.5;
      const minimumY = ground - bottomOffset;
      let velocityY = (pushableVelocities.get(asset.id) || 0) - 24 * dt;
      const nextY = asset.y + velocityY * dt;
      if (nextY <= minimumY) {
        asset.y = minimumY;
        velocityY = 0;
      } else asset.y = nextY;
      pushableVelocities.set(asset.id, velocityY);
      syncAssetTransform(asset);
    }
    updateFracturePhysics(dt);
  }

  function update(dt = 0) {
    const era = clamp(getEra(), 0, 1);
    // The editor backdrop is an editing aid only. Runtime scenes use their
    // production panorama, otherwise this rectangle follows the camera down
    // the shaft and obscures the elevator/laboratory art.
    const sceneOpacity = active && editorState.sceneArt.enabled ? editorState.sceneArt.opacity : 0;
    sceneArtRoot.visible = sceneOpacity > .005;
    if (sceneArtRecord.pastBackdrop) {
      sceneArtRecord.pastBackdrop.material.opacity = sceneOpacity * (1 - era);
      sceneArtRecord.pastBackdrop.visible = sceneArtRecord.pastBackdrop.material.opacity > .005;
    }
    if (sceneArtRecord.presentBackdrop) {
      sceneArtRecord.presentBackdrop.material.opacity = sceneOpacity * era;
      sceneArtRecord.presentBackdrop.visible = sceneArtRecord.presentBackdrop.material.opacity > .005;
    }
    if (sceneArtRecord.pastMesh) {
      sceneArtRecord.pastMesh.material.opacity = sceneOpacity * (1 - era);
      sceneArtRecord.pastMesh.visible = sceneArtRecord.pastMesh.material.opacity > .005;
    }
    if (sceneArtRecord.presentMesh) {
      sceneArtRecord.presentMesh.material.opacity = sceneOpacity * era;
      sceneArtRecord.presentMesh.visible = sceneArtRecord.presentMesh.material.opacity > .005;
    }
    for (const asset of editorState.assets) {
      const record = assetGroups.get(asset.id);
      if (!record) continue;
      if (fracturedAssets.has(asset.id)) {
        record.group.visible = false;
        continue;
      }
      syncAssetTransform(asset);
      let phase = animationPhases.get(asset.id);
      if (phase === undefined) phase = THREE.MathUtils.degToRad(asset.animation?.phase || 0);
      if (asset.animation?.type === 'rotate') {
        phase += THREE.MathUtils.degToRad(Number(asset.animation.speed) || 0) * dt;
        phase %= Math.PI * 2;
      } else phase = THREE.MathUtils.degToRad(asset.animation?.phase || 0);
      animationPhases.set(asset.id, phase);
      if (record.hitMesh) record.hitMesh.rotation.z = phase;
      if (record.pastMesh) {
        record.pastMesh.rotation.z = phase;
        record.pastMesh.material.opacity = asset.opacity * (1 - era);
        record.pastMesh.visible = record.pastMesh.material.opacity > 0.005;
      }
      if (record.presentMesh) {
        record.presentMesh.rotation.z = phase;
        record.presentMesh.material.opacity = asset.opacity * era;
        record.presentMesh.visible = record.presentMesh.material.opacity > 0.005;
      }
    }
    updateFractureAppearance(era);
    enforceHiddenWhitebox();
    updateSelectionOverlay();
    updateComponentMap();
  }

  loadEditorState();

  return {
    isActive: () => active,
    setActive,
    handleGameKeyDown,
    update,
    updateGamePhysics,
    fracturePhysicsAlongSegment,
    resolveHorizontalPlayer,
    resolveLandingY,
  };
}
