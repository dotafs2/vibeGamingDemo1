# 同一 SAO 世界的只读观察窗

这是当前 Godot 场景原型中的独立调试入口。它显示已有 SAO 存档中的真实身份、位置、目标、执行结果与库存；没有新的居民、没有自己的 Kimi 请求，也不写回世界。简化几何不是美术成品。当前社会执行和结算仍由已有 UE runtime 负责，不能把本查看器当作 Godot 社会迁移完成。

在仓库根目录执行（Python 3.11+；按本机位置设置 Godot）。新电脑没有完整world时，跳过导出命令，直接打开已提交的静态snapshot；这不是实时模拟：

```powershell
$godot = '替换为本机Godot可执行文件的完整路径'
python Prototypes/StartingTownWalkthrough/tools/export_sao_life_snapshot.py --world ThreeHearthsVillage/Saved/ThreeHearths/AincradLevel0/world.json --output Prototypes/StartingTownWalkthrough/validation/sao_life_snapshot.json
& $godot --path Prototypes/StartingTownWalkthrough res://sao_life_debug.tscn
```

窗口每两秒检查快照文件更新，R 手动刷新。FRESH/STALE 根据源存档时间计算；不断重新导出旧存档不能伪装为实时运行。`last_executed_result` 是实际行动结果，居民自己的 goal/need 只是意图。口粮和精力仅在 survival-v1 真正安装后显示。

持续同步需要单独运行 `tools/watch_sao_life_snapshot.py`，参数 `--world`、`--output` 与带 UTC 偏移的 `--stop-at-utc` 必填。同步器每至少5秒检查一次源变更，仅执行只读导出；截止前预留12秒结束，不启动游戏或付费请求。游戏必须由其独占运行者自行启动。

原生检查：

```powershell
& $godot --headless --path Prototypes/StartingTownWalkthrough res://sao_life_debug.tscn -- --validate --report validation-result.json
```

`--report` 建议使用绝对输出路径。检查实际载入同世界13身份、3活跃以及刷新不重复。若添加 `--capture <absolute.png>` 并使用正常渲染，可生成观察窗截图。实际本机检查与截图见 `validation/handoff_2026-09-10/`；它们不证明 NPC 已选择吃饭、休息或赠送材料，行为结论应核对正式世界事件与 Kimi 记录。

导出器拒绝写到正式世界同目录、源路径或硬链接别名。快照只包含白名单游戏字段，不含密钥、原始模型响应或预算配置。缺失美术 GLB 不阻断此调试入口，现有 V5 美术入口保持独立。
