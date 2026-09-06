# 屋面高光回归审计

旧 HearthCottage 的陶瓦粗糙度为 0.78，普通瓦和屋脊弧面使用平面法线，因此反射宽而弱、曲面反光分段。Polished 派生资产已把陶瓦改为 0.34 / 0.38 / 0.36 / 0.40，并平滑弧面、保留瓦片端部硬边。材质为非金属纯色 PBR；没有 Normal 贴图不等于不能产生高光。导出法线负责现有弧面高光，切线用于纹理法线坐标，不能用生成切线替代修复曲面法线。

此前另一个运行时问题是 Polished 副本保留 Nanite，而其导入材质未保存 Nanite usage，导致默认材质替代。该问题已通过关闭这些小型低模资产的 Nanite 修复。需检查实际 `/Game/ThreeHearths/Generated/HearthCottageRuntime/SM_HearthCottage_Polished`，不能只检查美术对照版本。

NPC 当前安装 `roof_slope_timber_2m` 木质屋面。其粗糙度为 0.48 / 0.51 / 0.53 / 0.50，颜色来自同库栗木梁，反光自然比陶瓦柔和。它保留陶瓦原型的弧面几何和法线；这属于复用几何的木质变体，不能把它描述成已经实现了不同材料的独立建造经济。

`source-audit.json` 是重新读取当前 GLB 字节流的结果。`native-audit.json` 由 UE Python 重新加载已导入资产生成；未生成该文件时，源检查不代表原生检查通过。脚本只读取资产，报告输出到本目录，不改地图、主存档或材质。

在仓库根目录复现源检查：

```powershell
python ThreeHearthsVillage/Plugins/ThreeHearths/Tools/audit_roof_materials.py
```

在无其他 UE 构建或测试的时段运行原生检查（完整编辑器初始化后自动退出）：

```powershell
& 'D:/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe' 'D:/Dev/vibeGamingDemo1/ThreeHearthsVillage/CropoutSampleProject.uproject' /Engine/Maps/Entry '-ExecutePythonScript=D:/Dev/vibeGamingDemo1/ThreeHearthsVillage/Plugins/ThreeHearths/Tools/audit_roof_materials.py' -unattended -nop4 -nosplash -NoSound -HearthDisableApi -HearthNoWorldPersistence -HearthRoofAuditQuit
```

检查源 PBR、UV 和单位法线、弧面平滑比例；原生模式另外比对全部材质 PBR、尺寸、三角形、UV、Nanite 关闭，记录 MikkTSpace/切线与法线构建设置。UE5.8 的 commandlet 不提供网格编辑子系统；该模式会明确标记构建设置未读取，需使用上述完整编辑器方式补齐。

实际原生资产的 `recompute_normals=true` 是观察项，不是自动判坏条件：它代表 UE 构建时重算法线，但平滑分组仍可能正确。当前审计没有逐顶点比较最终原生法线和源法线；因此不声称源法线原样保留，也没有盲目关闭该设置、重存网格。已有同光照 UE 图能确认 Polished 的可见连续高光改善。报告中的 `passed` 表示 PBR、材质绑定、几何和 Nanite 检查通过，法线构建观察保存在 `observations`；不代表任意相机/光照或当前游戏 GPU 像素的全面检查。

本轮重新查看的既有同光照 UE 对比图：

- [原始屋面](../HearthCottage/Polished_UE_roof_original.png)：瓦片弧面呈分段亮度，反光很弱。
- [Polished 屋面](../HearthCottage/Polished_UE_roof_polished.png)：瓦片出现连续亮带，端部仍为硬边。

这些是之前保存的原生截图，本轮没有重新拍摄或修改像素。主地图 Polished 运行时副本接入记录见 `Docs/Resident_Appearance.md`；原版资产与独立对比地图仍保留。
