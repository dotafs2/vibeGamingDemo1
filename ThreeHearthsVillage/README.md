# Three Hearths Village · 自主村庄

基于 Unreal Engine 5.8.2 和 Epic Cropout 示例的村庄原型。完整素材、地图、项目配置和插件源码放在这个独立目录中。

让新的开发 AI 接手时，可直接复制 [AI 接手提示词](AI_PROMPT.md)，并在末尾填写本次需求。

下一版的分步实现范围与验收见 [MVP 1 → 2 计划](Docs/MVP_1_to_2.md)。该文档是待实施规划。

原创低多边形小屋已导入默认村庄地图，位于北侧主路旁。场景对象名为 `Hearth Cottage - Shared UV`；修正后的 Blender 源文件、GLB、共享瓦片 UV、生成脚本及导入验证见 [房屋模型说明](Art/HearthCottage/README.md)。目前是带碰撞的场景外观，还未注册为 NPC 可入住的设施。

## 启动

1. 安装 Unreal Engine 5.8，以及可编译 UE C++ 插件的 Visual Studio 2022 C++ 工具链和 Windows SDK。
2. 下载或克隆整个仓库，保留本目录结构。直接打开 `CropoutSampleProject.uproject`；如提示编译插件，使用本机 UE 工具链编译。
3. 打开地图 `/Game/ThreeHearths/Maps/L_ThreeHearthsVillage`，点击编辑器播放。发布配置默认启动该地图。

也可以运行 `Open-Village.ps1 -EngineRoot "你的 UE 5.8 安装目录"`。初次编译可运行 `Build-Village.ps1 -EngineRoot "你的 UE 5.8 安装目录"`。

## 启用模型

默认未配置任何 API 密钥，村民使用本地规则也能运行。

1. 在本目录创建 `Saved/ThreeHearths/`。
2. 复制 `Plugins/ThreeHearths/Tools/kimi-config.example.json` 为 `Saved/ThreeHearths/api-config.json`。
3. 填写自己的 `api_key` 并设置 `enabled: true`，或在启动 UE 的环境中设置 `THREE_HEARTHS_API_KEY`。默认示例连接 `https://api.moonshot.cn/v1`，模型为 `kimi-k2.6`；可根据自己的服务账户更改。
4. 在游戏中点击“重新开始”加载配置。模型请求使用运行者自己的账户额度。

每名村民使用自己的人设和近期历史独立请求；一人等待或超时不会阻塞其他人。同一人最多一个未完成请求，生活与生产选择每人至少间隔 6 秒真实时间，全村每轮共享 600 次预算。达到上限后继续使用本地规则。

## 可以做什么

- 三名村民选址、取料、运输并建造最初的房屋。
- 开垦、建设四种农田、播种、收获、伐木、采石、采集浆果、种树、种灌木和扩建住宅，共 13 类生产建设操作。
- 休息、观察农田、巡查树林和拜访邻居。
- 每人独立历史，记录理由、结果、耗时和模型报告的 tokens。
- 共享资源账目、地块互斥、运输后入库、移动避让、暂停及 1～1000 倍速。

详情见 [生产技能](Plugins/ThreeHearths/Docs/生产与建设技能.md) 和 [角色历史](Plugins/ThreeHearths/Docs/角色决策历史.md)。

## 操作

空格暂停；数字 1/2/3 选择村民；WASD/方向键平移；滚轮缩放；F 跟随所选村民；Home 全景；R 重开。可以关闭自主生活，等当前任务结束后通过“安排工作”指定操作。手动安排不调用模型。

历史在重开后保留；世界存档尚未实现，重新开始或退出会重置房屋、库存和作物。扩建住宅目前不会增加人口，日常进食和作物腐烂尚未接入。

## 文件

- `CropoutSampleProject.uproject`、`Config/`：UE 项目和可移植配置。
- `Content/ThreeHearths/`：村庄地图、岛屿和材质。
- `Content/ThreeHearths/Generated/HearthCottage/`：已导入 UE 的房屋网格与材质。
- `Art/HearthCottage/`：原创房屋的 Blender/GLB 源资产、生成与验证脚本、UE 实拍及说明。
- `Plugins/ThreeHearths/`：新增 C++ 逻辑、界面、测试、配置示例。
- 其他 `Content/` 和示例插件：Cropout 场景、角色、动画及资源依赖；原始素材与插件作者信息保留。
- `Docs/Validation.md`：本版本实测结果及限制。

运行缓存、编译产物、真实 API 配置、账户凭证和个人决策历史不属于发布内容。未给第三方素材另行添加开源许可。
