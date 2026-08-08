import { randomUUID } from 'node:crypto';
import { promises as fs } from 'node:fs';
import path from 'node:path';
import { defineConfig } from 'vite';

const MAX_BODY_BYTES = 32 * 1024 * 1024;

function jsonResponse(response, status, payload) {
  response.statusCode = status;
  response.setHeader('Content-Type', 'application/json; charset=utf-8');
  response.end(JSON.stringify(payload));
}

function safeId(value, fallback = randomUUID()) {
  const normalized = String(value || '').replace(/[^a-zA-Z0-9_-]/g, '').slice(0, 96);
  return normalized || fallback;
}

async function readJsonBody(request) {
  const chunks = [];
  let size = 0;
  for await (const chunk of request) {
    size += chunk.length;
    if (size > MAX_BODY_BYTES) throw new Error('请求内容超过 32MB 限制');
    chunks.push(chunk);
  }
  return JSON.parse(Buffer.concat(chunks).toString('utf8') || '{}');
}

function decodeDataUrl(dataUrl) {
  const match = /^data:([^;,]+);base64,(.+)$/s.exec(String(dataUrl || ''));
  if (!match) throw new Error('图片数据格式无效');
  return { mime: match[1], bytes: Buffer.from(match[2], 'base64') };
}

function extensionForMime(mime) {
  if (mime === 'image/jpeg') return '.jpg';
  if (mime === 'image/webp') return '.webp';
  return '.png';
}

async function ensureEditorLayout(root) {
  const directories = [
    path.join(root, 'editor-data'),
    path.join(root, 'editor-jobs'),
    path.join(root, 'public', 'editor'),
    path.join(root, 'public', 'editor', 'assets'),
    path.join(root, 'public', 'editor', 'references'),
  ];
  await Promise.all(directories.map(directory => fs.mkdir(directory, { recursive: true })));
}

async function readEditorState(root) {
  const statePath = path.join(root, 'editor-data', 'editor-state.json');
  try {
    return JSON.parse(await fs.readFile(statePath, 'utf8'));
  } catch (error) {
    if (error.code !== 'ENOENT') throw error;
    return {
      schemaVersion: 3,
      references: { past: [], present: [] },
      assets: [],
      editor: { gridSize: 0.5, snapEnabled: true },
    };
  }
}

async function writeEditorState(root, state) {
  const serialized = `${JSON.stringify(state, null, 2)}\n`;
  await Promise.all([
    fs.writeFile(path.join(root, 'editor-data', 'editor-state.json'), serialized, 'utf8'),
    fs.writeFile(path.join(root, 'public', 'editor', 'editor-state.json'), serialized, 'utf8'),
  ]);
}

function editorPersistencePlugin() {
  let root = process.cwd();

  return {
    name: 'zero-echo-editor-persistence',
    configResolved(config) {
      root = config.root;
    },
    async configureServer(server) {
      await ensureEditorLayout(root);

      server.middlewares.use('/api/editor/state', async (request, response) => {
        try {
          if (request.method === 'GET') {
            jsonResponse(response, 200, await readEditorState(root));
            return;
          }
          if (request.method === 'POST') {
            const state = await readJsonBody(request);
            await writeEditorState(root, state);
            jsonResponse(response, 200, { ok: true });
            return;
          }
          jsonResponse(response, 405, { error: 'Method not allowed' });
        } catch (error) {
          jsonResponse(response, 400, { error: error.message });
        }
      });

      server.middlewares.use('/api/editor/reference', async (request, response) => {
        try {
          if (request.method !== 'POST') {
            jsonResponse(response, 405, { error: 'Method not allowed' });
            return;
          }
          const body = await readJsonBody(request);
          const image = decodeDataUrl(body.dataUrl);
          const slot = body.slot === 'past' ? 'past' : 'present';
          const id = safeId(`${slot}-${Date.now()}-${randomUUID().slice(0, 8)}`);
          const filename = `${id}${extensionForMime(image.mime)}`;
          await fs.writeFile(path.join(root, 'public', 'editor', 'references', filename), image.bytes);
          jsonResponse(response, 200, {
            id,
            slot,
            name: String(body.name || filename).slice(0, 160),
            url: `/editor/references/${filename}`,
          });
        } catch (error) {
          jsonResponse(response, 400, { error: error.message });
        }
      });

      server.middlewares.use('/api/editor/job-status', async (request, response) => {
        try {
          if (request.method !== 'GET') {
            jsonResponse(response, 405, { error: 'Method not allowed' });
            return;
          }
          const url = new URL(request.url, 'http://localhost');
          const jobId = safeId(url.searchParams.get('id'), 'invalid');
          const resultPath = path.join(root, 'editor-jobs', jobId, 'result.json');
          try {
            const result = JSON.parse(await fs.readFile(resultPath, 'utf8'));
            jsonResponse(response, 200, result);
          } catch (error) {
            if (error.code === 'ENOENT') {
              jsonResponse(response, 200, { status: 'pending' });
              return;
            }
            throw error;
          }
        } catch (error) {
          jsonResponse(response, 400, { error: error.message });
        }
      });

      server.middlewares.use('/api/editor/job', async (request, response) => {
        try {
          if (request.method !== 'POST') {
            jsonResponse(response, 405, { error: 'Method not allowed' });
            return;
          }
          const body = await readJsonBody(request);
          const jobId = safeId(body.jobId, '');
          const assetId = safeId(body.assetId, '');
          const versionId = safeId(body.versionId, '');
          if (!jobId || !assetId || !versionId) throw new Error('任务、资产或版本 ID 无效');
          if (!String(body.description || '').trim()) throw new Error('资产描述不能为空');
          const selection = decodeDataUrl(body.selectionImage);
          const jobDirectory = path.join(root, 'editor-jobs', jobId);
          const assetDirectory = path.join(root, 'public', 'editor', 'assets', assetId);
          await Promise.all([
            fs.mkdir(jobDirectory, { recursive: true }),
            fs.mkdir(assetDirectory, { recursive: true }),
          ]);

          const selectionFilename = `selection${extensionForMime(selection.mime)}`;
          await fs.writeFile(path.join(jobDirectory, selectionFilename), selection.bytes);

          const pastFilename = `${versionId}-past.png`;
          const presentFilename = `${versionId}-present.png`;
          const requestPayload = {
            schemaVersion: 3,
            status: 'pending',
            jobId,
            assetId,
            versionId,
            createdAt: new Date().toISOString(),
            description: String(body.description || '').trim(),
            selectionImage: path.posix.join('editor-jobs', jobId, selectionFilename),
            selectionWorldBounds: body.selectionWorldBounds,
            renderMode: body.renderMode || 'source-png',
            intent: body.intent === 'add' ? 'add' : 'replace',
            componentType: ['animation', 'physics', 'static'].includes(body.componentType) ? body.componentType : 'static',
            animation: body.animation || { type: 'none', speed: 0, phase: 0 },
            references: body.references,
            generationBrief: body.generationBrief,
            outputTargets: {
              pastFile: path.posix.join('public', 'editor', 'assets', assetId, pastFilename),
              presentFile: path.posix.join('public', 'editor', 'assets', assetId, presentFilename),
              pastUrl: `/editor/assets/${assetId}/${pastFilename}`,
              presentUrl: `/editor/assets/${assetId}/${presentFilename}`,
              resultFile: path.posix.join('editor-jobs', jobId, 'result.json'),
            },
          };
          await fs.writeFile(
            path.join(jobDirectory, 'request.json'),
            `${JSON.stringify(requestPayload, null, 2)}\n`,
            'utf8',
          );

          const relativeRequest = path.posix.join('editor-jobs', jobId, 'request.json');
          const requestedObject = String(body.description || '').trim().replace(/\s+/g, ' ');
          const requestIntent = body.intent === 'add' ? '添加' : '替换';
          jsonResponse(response, 200, {
            ok: true,
            jobId,
            requestFile: relativeRequest,
            command: `我刚在游戏场景里框选了一个位置，想在这里${requestIntent}：${requestedObject}。请处理资产编辑任务 ${relativeRequest}。位置、大小和邻接关系以 request.json 的 selectionWorldBounds 与框选截图为准；读取全部画风参考图，生成严格匹配当前场景的透明背景双时代 2D PNG，并按 docs/EDITOR_ASSET_WORKFLOW.md 写回 result.json。完成后资产必须自动出现在这个框的原位置。`,
          });
        } catch (error) {
          jsonResponse(response, 400, { error: error.message });
        }
      });
    },
  };
}

export default defineConfig({
  plugins: [editorPersistencePlugin()],
});
