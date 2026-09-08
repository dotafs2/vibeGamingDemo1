# 中世纪资源总览

`MedievalLife_LibraryOverview.png` 为30个静态模块的完整分层陈列图（5列×6行）。包含21个MedievalLife模块、4个FreightKit模块和5个MarketLifeKit模块；不是30个建筑，也不按材质或动画虚增数量。

根代理检查并修正了Luna初版的光照、文字朝向及高物体遮挡。最终按摄像机投影范围分别排列每个构件，保留其全部源网格层和材质。为方便浏览，各格独立旋转、等比缩放；这张图不用于比较游戏中的真实尺寸，也不是游戏截图。

配套 `.blend` 可编辑，JSON记录每个GLB来源、真实米制尺寸、展示缩放和三个目录哈希（MedievalLife、FreightKit、MarketLifeKit）。运行中的马车、门卫与护卫请看 `../RuntimeReview/`，人物和马匹的骨骼/动画清单分别位于 `../PeopleRigged/` 与 `../Rigged/`。

在Blender 5.2后台重现：

```text
blender -b --python-exit-code 1 --python Art/MedievalLife/create_library_overview.py -- --resolution 2600
```

脚本只读源GLB，在独立场景生成陈列图，不改源模型。最终30件图为2600×2908；运行日志 `Saved/ThreeHearths/MedievalReview/LibraryOverview30Reviewed.log`，北京时间2026-09-08 07:30完成并由根查看。根修正了新增第六排后标题压住工具架的问题，并增加上排柔和补光。原25件图的Render3日志不是当前最终结果。

## 真实尺寸合集

根于北京时间2026-09-08 08:25实际执行打包与独立FBX冷回导，`MedievalLife_All30_Audit.json` 为 passed：30模块、66网格层，父级、尺寸、材质、UV全部通过。FBX约4.29 MB。它包含本晚新增库，不包含项目过去所有建筑资源。

`export_complete_library.py` 读取三个现有声明，打包今晚新增库中的30件静态模块和66个网格层。合集按真实米制尺寸放在无地台、无相机、无灯光的5列×6行网格中，列宽和行深根据每个模块的实际 bounds 加间隔计算；每个模块的局部原点和所有分层材质、UV保持不变，父级空物体只记录网格展示位置并随 FBX 一起导出。输出为 `MedievalLife_All30.blend`、`MedievalLife_All30.fbx`、`MedievalLife_All30.glb` 和 JSON 清单。人物与马匹动画仍在各自的 Rigged 文件中，不重复计入这30件静态库。

在 Blender 5.2 后台生成并做 FBX 冷回导审计：

```text
blender -b --python-exit-code 1 --python Art/MedievalLife/export_complete_library.py
blender -b --python-exit-code 1 --python Art/MedievalLife/export_complete_library.py -- --audit
```

清单中的 `zeroing` 说明如何解除展示父级并把模块归零到其原始局部原点。该合集只覆盖今晚声明的30件资源，不声称包含仓库中的全部历史美术文件。
