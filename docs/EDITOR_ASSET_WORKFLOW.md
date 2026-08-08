# Codex 整景优先 / 双时代局部组件工作流

编辑器使用双时代岩壁底板，并把所有可操作设施显示为独立透明 PNG。Editor 内直接按住任意已有组件即可选择并拖动；点击空白区域再拖动才会画生成框并创建 `editor-jobs/<job-id>/` 任务包。

工具与快捷键：

- `V`：选择 / 移动；直接点击透明 PNG 的非透明像素。
- `B`：框选已有组件；框内有多个对象时列出候选。
- `F`：框选新增；始终建立 Agent 新增区域，即使从已有物体上开始拖动。
- `Ctrl+C` / `Ctrl+V`：复制、粘贴当前组件；`Delete`：删除当前组件；`Ctrl+D`：快速创建副本。

进入 Editor 后，组件地图会显示三类登记对象：`animation`（A，动画组件）、`physics`（B，物理组件）和 `static`（C，场景静态物）。画框覆盖多个对象时，先从候选列表选择唯一组件，再编辑或生成；只有选择“把手动画框作为新组件”时才使用整个框。

## Codex 必须完成的步骤

1. 完整读取 `request.json`、`selectionImage` 和其中列出的全部参考图。
   - `intent: "add"` 表示在框内新增用户描述的组件；框决定位置、尺寸与锚点，参考图的整体画风是硬约束。
   - `intent: "replace"` 表示替换或重新生成已存在的组件。
2. 根据 `description` 理解框内唯一组件，并使用框选截图锁定位置、轮廓、朝向和比例；不要重新生成整张场景。
3. 生成两张轮廓、画布、锚点和透明留白完全一致的高质量 2D PNG 组件：
   - `pastFile`：2047 年完整、仍在使用的版本。
   - `presentFile`：2147 年破败、老化后的同一物体。
4. 两张图都必须是透明背景 PNG、正交侧视、Unlit、没有环境光影、投影、地面或场景背景。
5. 保持框选白盒的主要轮廓与朝向；时代变化只改变损坏、锈蚀、缺失零件、污染和维护状态。
6. `renderMode` 为 `source-png`：保留生成图片的原始画质，禁止像素化、调色板量化、抖动、额外描边或有损降采样。
7. 若 `animation.type` 为 `rotate`，旋转轴必须位于画布几何中心，两版的旋转外轮廓必须稳定一致。
8. 读取 `componentType`：动画组件优先保证稳定轴心与轮廓，物理组件优先保证清楚且紧凑的碰撞轮廓，静态组件不添加不必要的活动零件。
9. 将文件写入 `outputTargets.pastFile` 和 `outputTargets.presentFile`。
10. 写入 `outputTargets.resultFile`：

```json
{
  "status": "complete",
  "jobId": "任务 ID",
  "versionId": "版本 ID",
  "pastImage": "/editor/assets/<asset-id>/<version-id>-past.png",
  "presentImage": "/editor/assets/<asset-id>/<version-id>-present.png",
  "completedAt": "ISO-8601 时间",
  "promptSummary": "实际采用的美术方向摘要"
}
```

若生成失败，写入：

```json
{
  "status": "failed",
  "error": "可读的失败原因"
}
```

编辑器会轮询 `result.json`；写入成功后，网页会自动加载并采用新版本。
