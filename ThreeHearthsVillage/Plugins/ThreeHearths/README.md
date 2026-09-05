# Three Hearths 插件

三名村民独立调用模型，执行选址建房、生产扩建、休息和拜访。当前人口为三人；请求名额按人数分配，同一村民最多一个未完成请求。全村每轮共享 600 次请求预算，生活与生产决策每人至少间隔 6 秒真实时间。

完整项目启动说明见 [项目 README](../../README.md)。

- [生产与建设技能](Docs/生产与建设技能.md)
- [角色决策历史](Docs/角色决策历史.md)
- [资源与模块清单](Docs/资源与模块清单.md)
- [素材清单](Docs/全部现有素材.csv)

入口文件：`Source/ThreeHearths/Public/HearthVillage.h`；实现位于 `Source/ThreeHearths/Private/`，包括村庄状态、HTTP 决策、生活历史、生产、移动避让和界面。原生自动化测试前缀为 `ThreeHearths`。

`Tools/kimi-config.example.json` 是默认关闭的无密钥示例。运行时配置和个人历史放在项目 `Saved/ThreeHearths/`，不提交到仓库。
