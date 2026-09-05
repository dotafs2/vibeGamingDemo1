# Hearth Cottage / 炉边小屋

为 ThreeHearths 小镇制作的原创低多边形外观模型：奶油色灰泥墙、陶红弧形瓦片、粗木梁、灰蓝色石基与烟囱、蓝绿色百叶窗、小门廊、花箱、柴堆及木桶。

**当前修正版：`HearthCottage_SharedUV.blend` 与 `HearthCottage_SharedUV.glb`。** 修复了重复瓦片各占一块 UV 的问题，模型几何与原有纯色材质保持一致。早期模型及打开窗口中的未保存操作已保留；请使用修正版继续编辑。

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
