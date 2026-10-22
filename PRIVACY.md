# 隐私说明

TalkBOX 是一个纯本地的 Windows 桌面话术库：不联网、不上传、不包含任何统计或遥测。

## 应用本身

- 不发起任何网络请求（没有 HTTP 客户端、没有更新检查、没有崩溃上报）。
- 不读取与话术无关的个人文件：只有你主动选择的图片 / PDF 会被复制到应用数据目录。
- 不写注册表除安装信息以外的内容（安装版会写入卸载信息，见下）。

## 数据保存在哪里

| 内容 | 位置 |
|---|---|
| 数据库（话术、分类） | `%LOCALAPPDATA%\TalkBOX\talkbox.db` |
| 附件副本 | `%LOCALAPPDATA%\TalkBOX\attachments\` |
| 备份文件 | `%LOCALAPPDATA%\TalkBOX\backups\`（仅在你手动备份时生成） |
| 运行日志 | `%LOCALAPPDATA%\TalkBOX\logs\talkbox.log`（2 MB 轮换，保留 5 代） |
| 窗口大小 / 位置 / 置顶状态 | `%LOCALAPPDATA%\TalkBOX\ui.ini` |
| 卸载信息（仅安装版） | `HKCU\Software\Microsoft\Windows\CurrentVersion\Uninstall\{7E0F5B5C-3B1E-4F1B-9E3A-2B4C6D8E5A10}_is1` |

日志只记录事件（启动、迁移、分类变更、拖拽自动发送结果、备份/恢复结果、错误），**不记录话术正文**。

## 剪贴板

点击卡片会把内容写入系统剪贴板（文本、图片位图或文件引用），这是应用的核心功能。除此之外不会读取剪贴板中的其他内容。

## 拖拽

把卡片拖到其他程序时，由 Windows 的 OLE 拖放机制传输内容，TalkBOX 不感知目标程序。

## 删除数据

卸载程序不会删除 `%LOCALAPPDATA%\TalkBOX`（避免误删你的话术库）。要彻底清除，卸载后手动删除该目录。
