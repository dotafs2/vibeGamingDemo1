# 美术资源范围与恢复说明

源码提交 `701694ab1398d63ef8413cf70ed2010b013b1f69` 上传了文本源码、配置和文档，没有包含本轮新增模型与贴图。不能把该次源码推送视为完整项目备份。

## 本次补充的资源范围

清单见 `authored_asset_manifest.json`：当前市场街 V5、托尔巴纳 V5 的 GLB 与 Blender 源，以及制作脚本依赖的 HQ、V3/V4 历史模型；原始城镇草模；ThreeHearths 自制美术源、导出物和 `Content/ThreeHearths` 下的原生美术资源。

清单共 2,645 个文件、1,974,430,222 字节（约 1.84 GiB）。包含 51 个 Blender 文件、385 个 GLB、39 个 FBX、341 个 PNG、1,821 个 UAsset 和 8 个地图。它是本地盘点快照，不意味着这些文件都是本次新增或已经上传。上传脚本只选择清单中 Git 显示为新增或修改的文件，为选中的二进制文件配置 Git LFS，不重写历史。

本次没有包含其他第三方 UE 素材目录、缓存、Saved、Blender 自动备份、无关网站和动画参考截图。已有远端资源不因为此次清单排除而删除。

## 桐人人物依赖仍为本地专用

当前运行时依赖 `assets/characters/kirito/Kirito.glb`。作者すがき随包 `readme.txt` 标明：原数据不能再分发；修改版再分发需告知作者。本项目没有记录已完成告知，继续保留原包、贴图及适配 Blender/GLB 的 Git 忽略规则，不借美术补传改变这一约定。来源与导入操作见 [人物接入说明](ExistingSAO/Engine_Integration.md) 及 [作者配布页](https://bowlroll.net/file/96677)。

因此新电脑即使取得本次环境美术，也仍需从作者处取得角色并按接入说明本地转换，才能运行当前默认人物场景。这里没有改动人物外观、删除本地文件或伪称干净克隆已通过运行验收。

## 上传与拉取验收

上传必须同时完成 LFS 内容传输和 Git 分支推送，并核对远端提交。LFS 用小指针记录大文件，浏览器里看见指针不代表模型丢失；取得项目时需安装 Git LFS 并执行 `git lfs pull`。[Git LFS 官方说明](https://git-lfs.com/)

本机手动上传入口：`C:\vibeGamingDemo1\tmp\level0-assets-upload-20260910\Push-Level0-Art.ps1`。脚本使用普通 Git，遵守当前执行入口；不修改 PATH、CodexProcessGate 或系统限制。任务内只进行了清单、哈希和脚本验证，实际远端结果以脚本生成的成功回执及用户终端输出为准。
