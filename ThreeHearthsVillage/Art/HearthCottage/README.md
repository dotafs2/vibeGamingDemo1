# Hearth Cottage / 炉边小屋

为 ThreeHearths 小镇制作的原创低多边形外观模型：奶油色灰泥墙、陶红弧形瓦片、粗木梁、灰蓝色石基与烟囱、蓝绿色百叶窗、小门廊、花箱、柴堆及木桶。

**共享 UV 基线：`HearthCottage_SharedUV.blend` 与 `HearthCottage_SharedUV.glb`。陶瓦外观新变体：`HearthCottage_SharedUV_Polished.blend` 与 `HearthCottage_SharedUV_Polished.glb`。** 基线保留原始材质与法线，供比较和回退；Polished 修复了陶瓦反光的材质和曲面法线。旧基线文件没有覆盖。新变体尚未经过 UE 导入验收，主地图目前仍使用旧版。

- `HearthCottage.blend`：可编辑源文件，模型按结构分组，附正交相机和展示灯光。
- `HearthCottage.glb`：只包含房屋与附属小道具，不包含展示地面、灯光及相机；材质为简单 PBR 纯色，不依赖外部贴图。
- `HearthCottage_Preview.png` / `HearthCottage_Rear.png`：正面与背面渲染。
- `create_cottage.py`：可复现建模脚本，使用固定随机种子；Blender 后台运行即可重建。
- `validate_cottage.py`：在空白 Blender 场景中重新导入 GLB，验证尺寸、面数、材质、退化三角面和外部依赖。
- `model-report.json` / `validation.json`：模型规格和验证结果。

Blender 5.2.1 LTS；16,788 个三角面，45 个网格对象，19 个材质。全包围盒约 4.40 × 4.26 × 3.80 米，包括台阶、瓦檐、柴堆和木桶。房屋墙体平面约 3.26 × 2.64 米。原点在地面中心；Blender 中正面为 -Y，导出使用标准 glTF Y-up 坐标转换。

当前交付为外观模型：没有可进入的室内，门不可开合。修正版已在 UE 5.8.2 编辑器中完成异步导入、材质与共享 UV 验证，并添加碰撞、放入村庄场景；尚未注册为 NPC 可入住的设施。

## UE 场景中的房屋

- 地图：`/Game/ThreeHearths/Maps/L_ThreeHearthsVillage`。
- 场景对象：`Hearth Cottage - Shared UV`，位于 `ThreeHearths/Generated` 文件夹。
- 网格：`/Game/ThreeHearths/Generated/HearthCottage/SM_HearthCottage_SharedUV`。
- 位置：`X=-1500, Y=1200, Z=2.8` 厘米，Yaw 为 90°；位于北侧主路旁，门口朝向主路。
- 45 个源网格合并为一个静态网格，保留 16,788 个三角面、19 个材质及 UV0。UE 内再次检查了屋顶 UV 的重复使用。
- 添加一个包围盒碰撞体，验证其会阻挡村庄的土地探测；九处地面采样确认房屋落地且没有占用道路。对象采用 Movable，以当前动态光照显示，共享 UV0 不用作烘焙光照贴图。
- 活动项目 `F:\CropoutSampleProject` 中的地图与新增资源已保存；相同资源和地图同步至此项目副本。

`HearthCottage_InUE.png` 为 UE 编辑器实拍。`UE_Import_Report.json` 记录导入源校验值、实际 UE 验证结果及同步资源校验值。此次验证范围是编辑器异步导入并保存为原生资源；游戏打包后的运行时导入还需单独实现与验证。

## 重复瓦片 UV 修复

原版依靠纯色材质显示外观：35 个网格带有基础几何体默认 UV，10 个手工生成网格（包括屋顶瓦片）没有 UV；原版并非完整展开后的贴图资产。

`HearthCottage_UV.blend` 和 `HearthCottage_UV_Layout.svg` 是已被替代的逐片自动展开演示，不应作为最终屋顶贴图布局。

修正版的材质坐标为 `UV_Material`，导出为 GLB 的 `TEXCOORD_0`：

- 182 片普通瓦片共享一个原型展开，13 片屋脊瓦共享另一个原型展开。
- 每个原型 6 个 UV 岛，总计 12 个基础岛；同规格、不同位置/朝向的瓦片 UV 完全一致。
- 半瓦、短瓦按实际尺寸使用原型图块的一部分，保持方向及纹理比例，不再独占图集空间。
- 颜色差异继续使用原有材质色，不需要给每片瓦分配唯一 UV。
- 其他建筑部件使用独立的材质布局。材质 UV 中的重叠是刻意复用；它不是光照贴图通道，未来如需静态光照烘焙应另建不重叠的 lightmap UV。

`shared_roof_uv.py` 是共用实现，已接入 `create_cottage.py`，重新生成也会得到修复后的 UV。`repair_shared_uv.py`（兼容入口 `prepare_uv_view.py`）可从已保存的原模型生成修正版，不重建几何。`HearthCottage_SharedUV_RoofUV.svg` 显示共享屋顶 UV；对应的 `*_uv-report.json`、`*_uv-validation.json` 记录重复实例匹配与 GLB 导出验证。

检查导出后 UV 是否仍复用：

```powershell
blender --background --factory-startup --threads 2 --python-exit-code 1 --python validate_shared_uv.py
blender --background --factory-startup --threads 2 --python-exit-code 1 --python validate_cottage.py -- HearthCottage_SharedUV
```

## 重建

在本目录执行（`blender` 需指向本机 Blender 可执行文件）：

```powershell
blender --background --factory-startup --threads 4 --python-exit-code 1 --python create_cottage.py
blender --background --factory-startup --threads 4 --python-exit-code 1 --python validate_cottage.py
```

仅重建模型、不渲染预览时，在建模命令末尾增加 `-- --no-render`。脚本会覆盖本目录中对应的模型与预览输出，手工修改后请先另存源文件。

## 陶瓦高光修正 / Polished

从实际保存的 SharedUV 源文件测得，4 个屋顶材质的 Roughness 均为 0.78，Metallic 均为 0；182 片普通瓦和 13 片屋脊的所有表面都使用平面法线。前者把反射摊得很宽，后者让每个弧面分段拥有突变的法线，缺少沿瓦面移动的连续反光。Normal 和 Roughness 输入也没有贴图，但纯色 PBR 本身完全可以产生正确高光，不能把缺少贴图当作唯一原因。旧屋顶底色比现有 VillageKit 陶瓦更浅、更粉，这属于另一个外观差异。

`polish_cottage_roof.py` 从保留的 SharedUV `.blend` 开始，只进行以下修正：

- 4 个陶瓦粗糙度改为 0.34、0.38、0.36、0.40，金属度仍为 0。
- 普通瓦的上下弧面和屋脊内外弧面使用平滑法线；端面、侧边保留硬边，并明确标记边界。未新增圆角或任何几何。
- 屋顶底色使用现有 VillageKit 的 `BB695F / CA7C6F / AF6059 / D28B79`。仅屋顶改色，其余材质完全保留。
- IOR、Specular IOR Level、Coat Weight、灯光、世界环境、曝光和色彩管理保持原值，没有用金属或额外涂层伪造反射。

几何仍为 45 个网格、19 个材质、16,788 个三角面。全部顶点、三角形材质分配、节点变换、包围盒与共享 UV0 均与基线一致；不重新展开、不复制独占瓦片 UV。`validate_polished_cottage.py` 独立读取两个 GLB 字节流，按每个三角形的坐标、UV 和材质对照，并检查单位法线、退化面、非屋顶法线及材质不变。检查结果和原文件 SHA-256 分别记录在 `*_validation.json`、`*_repair-report.json`、`*_diagnosis.json`。

对比图是原模型自带三盏灯、同一世界环境、同一相机/曝光的 Blender Cycles 实际渲染，没有修改图片像素：

- `*_house_original.png` 与 `*_house_polished.png`：全屋前后。
- `*_roof_original.png`：原始粗糙度、原始平面法线与旧颜色。
- `*_roof_finish_only.png`：只修粗糙度与法线，仍保留旧颜色，隔离高光改善效果。
- `*_roof_polished.png`：相同修正再加与 VillageKit 一致的陶瓦底色。

这里的 `*` 为 `HearthCottage_SharedUV_Polished`。每张图附有 `_render.json`，记录相同灯光哈希、几何/UV 指纹、相机、样本数与输出哈希。全屋和屋顶近景是两个固定机位；每组内部相机不变。`HearthCottage_Polished_Comparison.html` 可在本地切换三个屋顶状态，并同时查看全屋前后。原生 UE 光照下的最终效果由后续导入单独验收。

通过 Blender MCP `execute_blender_code` 执行以下代码生成变体；该步骤不会渲染，也不会覆盖原 SharedUV 文件：

```python
import runpy
repair = runpy.run_path('D:/Dev/vibeGamingDemo1/ThreeHearthsVillage/Art/HearthCottage/polish_cottage_roof.py')
repair['build']()
```

每个渲染单独一次 MCP 调用：`repair['render'](STATE, VIEW)`，STATE 为 `original / finish_only / polished`，VIEW 为 `house / roof`。渲染同样从原文件重读，避免把上一次状态带入下一次对比。最后用普通 Python 执行 `validate_polished_cottage.py`。现有 `create_cottage.py` 与旧 UV 修复入口没有修改；它们继续生成原始基线，Polished 由新脚本派生。
