# TalkBOX · 本地话术库（Qt 6 原生 Windows 桌面版）

[![Release](https://img.shields.io/github/v/release/Jonxinxin/TalkBOX)](https://github.com/Jonxinxin/TalkBOX/releases/latest)
[![Platform](https://img.shields.io/badge/platform-Windows%2010%2F11-0078D4)](#系统要求)
[![License](https://img.shields.io/badge/license-see%20LICENSES-lightgrey)](#开源许可)

面向销售与客服场景的本地话术库：文本话术一键复制，图片 / PDF 集中托管。
数据全部保存在本机，离线运行，无需账号或网络。

技术路线：**Qt 6 + Qt Quick/QML + C++20 + CMake + SQLite（QSQLITE）**。

> 需求与视觉参考的原型资料不随源码公开（仅保留在本机）。

> 本仓库的 `main` 分支是 **1.0 起的 Qt 重写版**。旧版（PySide6）保留在
> [`pyside6-legacy`](https://github.com/Jonxinxin/TalkBOX/tree/pyside6-legacy) 分支，
> 对应的 0.0.1–0.0.4 安装包仍在 Releases 里可下载。

## 下载安装

| 方式 | 说明 |
|---|---|
| **安装版**（推荐） | 从 [Releases](https://github.com/Jonxinxin/TalkBOX/releases/latest) 下载 `TalkBOX-<版本>-setup.exe`，双击安装。当前用户安装，**不需要管理员权限**；卸载不会删除你的话术数据 |
| **免安装版** | 下载 `TalkBOX-<版本>-portable.zip`，解压后双击 `TalkBOX.exe` 即用 |

首次启动是空库：分类完全由你自己创建（`设置 → 分类管理`）。从旧版（PySide6）升级的用户可参考
[`docs/migrate-from-legacy.md`](docs/migrate-from-legacy.md)。

<p align="center">
  <img src="docs/app-main.png" width="46%" alt="话术列表">
  <img src="docs/app-new-snippet.png" width="46%" alt="新建话术">
</p>
<p align="center">
  <img src="docs/app-settings.png" width="46%" alt="设置">
</p>

## 系统要求

- Windows 10 1809+ / Windows 11（x64）
- 无需安装 .NET、Python 或 Qt 运行库（已随包部署）
- 约 60 MB 磁盘（安装版）+ 数据目录占用

## 当前进度（阶段 A / B / C / D 已完成）

**阶段 A：数据闭环**
- [x] Qt Quick/QML + C++20 + CMake 工程骨架
- [x] 应用数据目录、SQLite schema 与版本迁移（PRAGMA user_version）
- [x] 分类完全由用户创建（**无内置分类**），未分类内容通过虚拟「未分类」筛选查看
- [x] `QAbstractListModel` 话术列表 + 分类列表模型
- [x] 新建文本话术（校验、事务落库）、重启后数据保留
- [x] 点击卡片复制**完整正文**（键盘 Enter/Space 同样有效）
- [x] 双列卡片、搜索（150ms 防抖）、分类筛选、类型筛选（组合生效）
- [x] 空库 / 搜索无结果空状态、成功 / 失败轻提示、启动失败兜底窗口
- [x] 日志（`logs/talkbox.log`，2MB 轮换，不记录话术正文）

**阶段 B：附件与操作菜单**
- [x] 新建流程第一步选择 文本 / 图片 / PDF
- [x] 图片导入（PNG/JPG/JPEG/WEBP/BMP，≤50 MB）与 PDF 导入（≤100 MB，校验 %PDF 头）
- [x] 文件异步复制到 `attachments/`（UUID 存储名，不阻塞 UI；失败清理半成品）
- [x] 图片卡片缩略图 + 文件名 + 类型/大小标签；缩略图失败显示占位与"文件缺失"
- [x] 应用内图片预览：复制图片 / 打开原图 / 在文件夹中显示；弹窗宽高自适应面板，按钮不会溢出窗口
- [x] **卡片主体点击 = 直接复制**：文本复制全文、图片复制位图、PDF 复制文件引用（可 Ctrl+V 粘贴到聊天窗口）
- [x] 预览 / 打开等操作改到**右键或右上角「…」菜单**；菜单点击不触发卡片主体行为，Esc / 点击外部关闭
- [x] 三点菜单按类型呈现：复制内容 / 复制图片 / 复制文件 / 预览 / 打开 / 编辑内容 / 重命名 / 修改分类 / 在文件夹中显示 / 删除
- [x] 编辑文本、重命名（只改显示名，保留扩展名，禁止路径分隔符）、改分类（即时生效）
- [x] 删除二次确认（取消为默认焦点）；数据库记录与附件文件引用计数清理策略
- [x] 未保存修改的二次确认；导入中禁关闭；保存失败不产生孤立文件

**阶段 C：数据可靠性**
- [x] 分类管理：新建 / 重命名 / 删除（删除时内容自动变成「未分类」并提示影响数量）
- [x] 分类名校验：去首尾空白、非空、不区分大小写与首尾空格的重名校验
- [x] 分类使用低饱和配色循环分配；分类为空时界面给出新建提示（迁移 v3 移除全部内置分类与「未分类」行）
- [x] 设置页（齿轮入口）：数据备份 / 分类管理 / 关于（数据目录、日志目录、版本、内容统计）
- [x] 一键备份为单个 `.talkboxbackup`（ZIP：manifest.json 清单 + `VACUUM INTO` 数据库快照 + attachments 附件）
- [x] 备份异步执行（不阻塞 UI），带进度与状态文字
- [x] 恢复前校验：文件类型、manifest、格式版本、路径穿越防护、数据库版本 / 完整性 / 清单计数、附件齐全
- [x] 恢复采用暂存目录校验 → 通过后才替换；替换前自动把当前数据完整保留到 `backups/pre-restore-<时间戳>/`
- [x] 恢复失败自动还原原数据，原数据始终保持可用
- [x] 日志轮换提升到 5 代（2MB/代），记录迁移 / 分类变更 / 备份恢复事件，且不记录话术正文

**辅助面板体验（按用户反馈调整）**
- [x] 默认窗口改为**紧凑侧边面板**（420 × 约 780 DIP，首次运行停靠屏幕右侧），不再占用大半个屏幕
- [x] 卡片列数随宽度自适应：≥ 900 DIP 三列、≥ 360 DIP **双列**（默认面板即为双列）、更窄时单列
- [x] **鼠标左键拖拽卡片**：文本卡片拖到聊天窗口（微信等）直接得到正文；图片 / PDF 以文件形式拖出（可被聊天工具作为附件接收）
- [x] 拖拽不影响点击：未移动时点击即复制内容（文本 / 图片 / PDF 文件），拖动后不会误触发复制
- [x] **拖拽内容反馈**：拖拽时卡片浮起（描边加深、内容淡化），底部浮条提示"松开即可把文字 / 文件拖到聊天窗口"并显示被拖内容摘要，指针变为复制光标
- [x] **点击弹窗外部关闭**：新建 / 编辑 / 重命名 / 修改分类 / 删除确认 / 图片预览 / 设置等弹窗，点击面板外软件内任意空白处或按 Esc 即可关闭；有未保存内容时改为弹出"放弃"确认，导入 / 备份进行中不会被误关闭
- [x] Windows 窗口缩放条带：如实记录——窗口放大（尤其从左边/底部拖拽）时新露出的区域仍可能短暂显示桌面底色，属于 Qt Quick 场景图在 Windows 上的已知行为（QTBUG-46074 一类），当前已用整窗不透明页面底色层将影响降到最低
- [x] **类型筛选移到底部**：全部内容 / 图片 / PDF 三个筛选按钮从列表上方移到窗口底部，列表区域完整用于卡片；顶部只保留分类筛选与内容计数
- [x] **卡片更紧凑**：卡片高度 118 → 96 DIP，内边距与缩略图同步收紧，文字卡片底部不再留大片空白
- [x] **自定义标题栏 + 原生窗口样式**：保留 Windows 原生窗口框架（`WS_CAPTION | WS_THICKFRAME`），由 DWM 提供投影、圆角（Windows 11）、缩放边框与 Aero 贴边；`WM_NCCALCSIZE` 只移除标题栏的可视区域，标题栏改由 QML 绘制（应用图标 + 名称 + 最小化 / 最大化 / 关闭），关闭按钮悬停变红。焦点闪黑已消除（窗口按钮 `Qt.NoFocus`、`WM_NCACTIVATE` 直答、DWM 边框色与页面描边一致），详见 `src/platform/WindowChrome.cpp`
- [x] **类型筛选栏**：结果计数左对齐、全部内容 / 图片 / PDF 按钮右对齐，分居两端；列表上方留出与分类标签的呼吸间距，第一排卡片不再贴住分类标签
- [x] **置顶开关**（标题栏右侧图钉），方便悬浮在微信旁边使用
- [x] 记住窗口大小、位置与置顶状态（`ui.ini`），下次启动原样恢复（最大化 / 最小化时保存的是还原后的尺寸）
- [x] 图标改用 **Material Symbols**（Apache-2.0）+ Bootstrap Icons 图钉（MIT），见 `LICENSES/`

**阶段 D：发布准备**
- [x] exe 内置应用图标（多尺寸 `.ico`）与版本信息（资源文件由 CMake 版本号注入）
- [x] `scripts/package.ps1` 一键产出**免安装 zip** 与 **Inno Setup 安装包**（当前用户安装、无需管理员权限）
- [x] 安装包卸载不删除 `%LOCALAPPDATA%\TalkBOX`；覆盖安装保留数据（已实测升级 → 卸载流程）
- [x] 发布体积裁剪：移除未使用的 Qt Quick Controls 样式、qmltooling、多余 SQL 驱动与非中英文翻译（zip 28.4 → 23.7 MB，安装包 18.9 → 17.1 MB）
- [x] 干净目录冒烟验证（解压 → 首启建库 → 迁移 v1/v2/v3 → 窗口正常创建）
- [x] `PRIVACY.md`（纯本地、无网络、无遥测）与 `docs/` 发布文档

## 系统要求

- Windows 10 1809+ / Windows 11（x64）
- 需要 x64 平台；不支持 32 位与 ARM64（未做验证）

## 开发环境（本机已配置）

| 组件 | 版本 / 位置 |
|---|---|
| Windows | 10 22H2（19045）x64，系统缩放 125% |
| Qt | 6.8.3 MSVC2022 64-bit，`C:\Qt\6.8.3\msvc2022_64` |
| 编译器 | MSVC v143（VS 2022 Build Tools，`C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools`） |
| CMake | 4.4.4（`Kitware.CMake`，winget 安装） |
| Ninja | 1.13.2（备用，当前构建使用 VS 生成器） |

## 构建与运行

```powershell
# 配置（首次或 CMakeLists 变更后）
cmake --preset windows-msvc

# 构建
cmake --build build/windows-msvc --config Debug
cmake --build build/windows-msvc --config Release

# 运行测试
ctest --test-dir build/windows-msvc -C Debug --output-on-failure
ctest --test-dir build/windows-msvc -C Release --output-on-failure

# 开发期运行（需要 Qt 运行库在 PATH 中）
$env:PATH = "C:\Qt\6.8.3\msvc2022_64\bin;$env:PATH"
build\windows-msvc\Debug\TalkBOX.exe     # 或 Release\TalkBOX.exe
```

- 产物：`build/windows-msvc/Debug/TalkBOX.exe`、`build/windows-msvc/Release/TalkBOX.exe`、
  `build/windows-msvc/<配置>/tst_*.exe`。
- 测试可执行文件的环境变量已由 CMake 配置（自动加入 Qt `bin` 目录），直接 `ctest` 即可。

## 打包与发布

```powershell
# 构建 + windeployqt + 生成 zip 和安装包（需要 Inno Setup 6）
pwsh scripts/package.ps1

# 只构建（安装包缺失时脚本会给出提示并跳过）
pwsh scripts/package.ps1 -Configuration Release
```

产物：

```text
dist\TalkBOX-1.0.0-setup.exe       # 安装包（当前用户安装，无需管理员权限）
dist\TalkBOX-portable-1.0.0.zip    # 免安装版
dist\TalkBOX\                      # 已部署的目录（打包中间产物）
```

- 版本号只在 `CMakeLists.txt` 的 `project(... VERSION ...)` 维护，exe 版本信息、安装包文件名、zip 名都由它派生。
- 安装包需要 Inno Setup 6：`winget install JRSoftware.InnoSetup`（或用 `ISCC_PATH` 指向 `ISCC.exe`）。
- 安装包脚本在 `packaging/talkbox.iss`，中文语言文件在 `packaging/languages/`。
- 脚本会裁掉本应用用不到的 Qt 组件（非 Basic 的 Controls 样式、qmltooling、多余 SQL 驱动、非中英文翻译），
  因此修改 QML 样式或新增import 后请重新跑一次打包并冒烟验证。

发布流程：打 tag（`v1.0.0`）→ 跑一次打包 → 把两个产物上传到对应 GitHub Release。

> 直接运行 `build\...\Release\TalkBOX.exe` 而不部署会提示缺少 Qt DLL（这是正常的开发产物行为）。

## 数据位置

```text
%LOCALAPPDATA%\TalkBOX\
├─ talkbox.db        # SQLite 数据库（WAL 模式）
├─ attachments\      # 托管附件
├─ backups\          # 备份输出（含覆盖安装前的自动安全副本）
├─ logs\talkbox.log  # 运行日志（2MB 轮换，保留 5 代）
└─ ui.ini            # 窗口大小 / 位置 / 置顶状态
```

- 表结构：`snippets`、`categories`、`app_settings`，schema 版本由 `PRAGMA user_version` 管理（当前 v3）。
- 可用环境变量 `TALKBOX_DATA_DIR` 覆盖数据目录（用于测试 / 便携运行）。
- 卸载程序**不会**删除这个目录，避免误删话术库。

## 工程结构

```text
├─ CMakeLists.txt / CMakePresets.json   # 版本号唯一来源
├─ assets/                              # talkbox.ico + Windows 资源（图标 / 版本信息）
├─ packaging/                           # Inno Setup 脚本与中文语言文件
├─ scripts/package.ps1                  # 一键打包（zip + 安装包）
├─ docs/                                # 发布说明、旧版迁移、界面截图
├─ src/
│  ├─ main.cpp                 # 启动、数据库初始化、QML 单例注册、兜底错误窗口
│  ├─ app/                     # AppPaths（数据路径）、Logging（文件日志轮换，5 代）
│  ├─ data/                    # DatabaseManager、Migrations（v1/v2/v3）、Snippet/Category Repository
│  ├─ models/                  # SnippetListModel、CategoryListModel（QAbstractListModel）
│  ├─ platform/                # WindowChrome：原生框架 + DWM 投影 + 自定义标题栏
│  └─ services/                # SnippetService、AttachmentService、SystemService、
│                              # CategoryService（分类管理）、BackupService（备份/恢复）、
│                              # AppInfo、ClipboardWriter
├─ qml/
│  ├─ Main.qml / Theme.qml / StartupError.qml
│  ├─ components/              # TitleBar、CaptionButton、SearchField、CategoryChip、
│  │                           # CategoryPicker、SnippetCard、CardMenuItem、AppButton、
│  │                           # Toast、类型筛选、空状态
│  ├─ dialogs/                 # NewSnippetDialog（两步式）、EditTextDialog、RenameDialog、
│  │                           # ChangeCategoryDialog、DeleteConfirmDialog、ImagePreviewDialog、
│  │                           # SettingsDialog（备份/分类/关于）、ConfirmDialog、TextInputDialog
│  └─ icons/                   # SVG 图标（app.svg 为自有应用图标）
└─ tests/
   ├─ unit/                    # 8 组单元测试
   └─ integration/             # tst_ui_flows（真实 QML + 合成事件）
```

架构约定：QML 只做界面；SQL 与业务全部在 C++（Repository / Service / Model），
QML 通过 `TalkBox` 模块单例（`Snippets`、`Categories`、`SnippetService`、`AppInfo`）访问。

## 已知限制

- OCR 图片文字搜索、PDF 内容全文索引、全局快捷键（P1）未实现。
- 搜索为 SQL `LIKE` 子串匹配（已转义 `% _ \`）。
- 列表刷新使用模型重置（数据量大时后续可优化为增量更新）。
- 大图预览受 `sourceSize` 限制（约 1400×1000），保证内存稳定。
- 备份恢复在后台线程执行；超大附件较多时（单文件 >100 MB）解压阶段会占用相应内存。
- 仅提供 x64 构建；未在 ARM64 与 32 位 Windows 上验证。
- 从左侧 / 底部拖拽放大窗口时，新露出的边缘可能短暂显示桌面底色（Qt Quick 场景图在 Windows 上的已知行为，QTBUG-46074 一类）。

## 开源许可

- Qt（LGPL-3.0 / 商业双许可）：发布包内附带 Qt 动态库，可按 LGPL 要求自行替换。
- 图标：Material Symbols（Apache-2.0）、Bootstrap Icons 图钉（MIT），全文见 `LICENSES/`。
- 应用图标 `qml/icons/app.svg` 为本项目自有素材。
- 本仓库源码未附加额外许可声明；如需在他人项目中复用，请先联系作者。

## 隐私

纯本地运行、不联网、无遥测。数据存放位置与日志策略见 [`PRIVACY.md`](PRIVACY.md)。

## 测试覆盖

| 测试 | 覆盖内容 |
|---|---|
| `tst_database` | schema 创建、无内置分类（启动即为空分类列表）、user_version、重复迁移幂等、外键约束、v3 升级（删除旧内置分类并把其内容变为未分类） |
| `tst_snippet_repository` | 插入 / 查询 / 更新 / 删除、排序、正文与分类名 / 文件名搜索、组合筛选、未分类筛选、LIKE 通配符转义、附件引用计数 |
| `tst_snippet_service` | 文本校验与复制、附件记录创建（扩展名补齐）、编辑 / 重命名 / 改分类（含清空分类）、删除时文件引用计数与清理失败告警 |
| `tst_attachment_service` | 导入复制与唯一命名、格式 / 大小 / PDF 头 / 空文件 / 损坏图片校验、路径穿越防护、删除幂等 |
| `tst_system_clipboard` | 真实系统剪贴板的图片复制、文件引用（CF_HDROP）复制（环境不可用自动跳过）、文件缺失错误路径 |
| `tst_category_repository` | 起始无分类、分类新建/重命名/删除、重名与空名校验、删除时内容变为未分类与计数、配色与排序 |
| `tst_backup_service` | 备份容器（manifest/数据库快照/附件）、恢复往返与安全副本、非法路径防护、非 ZIP / 缺清单 / 版本不符 / 数据库损坏 / 清单计数不符等拒绝场景、失败后原数据完好 |
| `tst_persistence` | 关闭后重新打开（模拟重启）数据与 schema 完整、分类不会重复 |
| `tst_ui_flows` | UI 集成（真实 QML + 合成事件）：卡片点击即复制（文本 / 图片 / PDF 文件三个类型）、右键打开菜单且不复制、菜单不冒泡与菜单项操作（AT-04）、图片预览经菜单打开并可复制、预览弹窗按钮不被窄面板裁切、删除取消/确认与附件清理（AT-11）、双列网格断言、设置页分类管理全流程（新建/重命名/删除）、无内置分类与「未分类」筛选、点击弹窗外部关闭（含嵌套弹窗不影响父弹窗、有未保存内容时转为放弃确认）、拖拽内容反馈浮条出现与隐藏、计数与筛选按钮左右分列、卡片四边留白及左右对称、自定义标题栏（贴顶、按钮顺序与齐右、按钮不取焦点、最大化不越出工作区、还原图标切换与还原）、启动与新建弹窗的焦点描边（初始无描边、悬停有、移开消失、方向键描边当前卡片、鼠标点击清除、鼠标打开弹窗无描边 / Tab 后出现）。桌面锁定时自动跳过 |

## 贡献与反馈

- 发现问题或有功能建议：直接开 [Issue](https://github.com/Jonxinxin/TalkBOX/issues)。
- 提交代码前请先跑通 `ctest --test-dir build/windows-msvc -C Release --output-on-failure`（9 组测试）。
- 数据格式变更必须新增一条迁移（`src/data/Migrations.cpp`）并提升 `kTargetVersion`，同时补对应测试。

## 变更记录

- 2026-10-20 旧版数据迁移：旧版本用 `talk_data.json` + `images/` + `pdfs/` 存话术，新版用 SQLite。提供一次性转换脚本按内容 SHA-256 复制附件到 `attachments/<uuid>.<ext>`、按 JSON 里出现的分类名自动新建分类（配色按调色板循环分配），并保留原有的排列顺序；导入前把新库与附件完整备份到 `backups/pre-legacy-import-<时间戳>/`，旧目录只读不修改。转换工具脚本见 `docs/migrate-from-legacy.md` 的字段映射说明（迁移工具不随发布包分发）。

- 2026-10-20 移除全部内置分类：分类改成完全由用户创建，软件不再预置「开场 / 电表 / 报价 / 售后 / 未分类」。①**数据层**：新增迁移 v3（`kTargetVersion = 3`）——重建 `snippets` 表把 `category_id` 改为可空（`ON DELETE SET NULL`）、删除 `is_builtin = 1` 的分类行、并把原本挂在内置分类下的内容置为 `NULL`（即「未分类」），用户自己建的分类与其内容不受影响；`Category` 去掉 `isBuiltin`、`CategoryListModel` 去掉 `isBuiltin` 角色、`CategoryRepository::uncategorizedId()` 与 `CategoryService::canManage()` 一并移除。②**未分类改为虚拟项**：`snippets.category_id IS NULL` 表示未分类，筛选键为 `SnippetFilter::uncategorizedKey()`（`"uncategorized"`，与 UUID 不会冲突）；`SnippetListModel` 新增 `uncategorizedCount` 属性，顶部只在确实存在未分类内容时显示「未分类」标签，卡片上的分类标签在无分类时显示「未分类」并用中性石板色。③**新建 / 修改分类**：分类选择器（`CategoryPicker`）新增虚拟「未分类」选项；分类列表为空时提示可在「设置 → 分类管理」中新建，新建内容时也提示可以先不分类保存；`SnippetService::changeCategory` 允许把内容改回未分类。④**设置页**：去掉「内置」标记与对应的保护逻辑，删除提示改为「该分类下的 N 条内容会变成「未分类」」，分类列表为空时给出空状态提示。⑤**测试**：数据库 / 仓储 / 服务 / 备份 / 持久化测试改为各自创建所需分类（并新增「启动即为空分类列表」「未分类筛选」「v3 升级丢弃旧内置分类并把内容变为未分类」等断言），`tst_ui_flows` 新增无内置分类与「未分类」标签的 UI 断言。9 组测试 Debug 全通过。

- 2026-10-20 自定义标题栏（原生窗口样式 + DWM 投影 + QML 标题栏）：移除系统标题栏的可视区域而不是移除窗口框架——`WindowChrome`（`src/platform/WindowChrome.cpp`）在 `WM_NCCALCSIZE` 中把客户区设为整窗，同时保留 `WS_CAPTION | WS_THICKFRAME`，因此投影、缩放边框、Aero 贴边、系统菜单、窗口动画仍来自 Windows。①**DWM 投影**：`DwmExtendFrameIntoClientArea(1,1,1,1)` 把一像素不可见框架交还 DWM（由不透明 QML 内容覆盖），投影是系统投影，不再使用 QML 模糊层，也就没有"假阴影"与边缘黑带；`DWMWA_NCRENDERING_POLICY` 显式启用，`DWMWA_BORDER_COLOR` 与页面描边同色，`DWMWA_WINDOW_CORNER_PREFERENCE` 在 Windows 11 上给出圆角（Windows 10 忽略）。②**Qt 会重设窗口样式**：Qt 6.8 在创建窗口、`flags` 变化（置顶开关）、状态变化时会按 `QWindow::flags()` 重写 `GWL_STYLE`，Windows 也会在 DPI / 主题变化时重设框架，所以不能只在构造时调用一次 DWM API——`WM_STYLECHANGED` / `WM_THEMECHANGED` / `WM_DPICHANGED`、`WinIdChange` / `Show` / `WindowStateChange` 都会重新断言框架位与全部 DWM 属性（排队到事件循环空闲时执行，避免在样式变更消息里递归修改）。③**焦点闪黑**：标题栏按钮 `focusPolicy: Qt.NoFocus`（工具栏 `IconButton` 改为 `Qt.TabFocus`，鼠标点击不取焦点），`WM_NCACTIVATE` 直接返回 `TRUE` 不再触发非客户区重绘，DWM 边框固定为浅色、不随激活态切换。④**最大化**：原生框架在最大化时会外扩到工作区之外，`WM_NCCALCSIZE` 在最大化时把客户区夹回 `rcWork`，标题栏始终贴在屏幕顶部而不是被顶出屏幕。⑤**标题栏**：新增 `qml/components/TitleBar.qml`、`CaptionButton.qml` 与 `window-*.svg` / `app.svg`；高度 42 DIP，左侧图标 + "TalkBox 话术库"，右侧最小化 / 最大化（还原时换成还原图标）/ 关闭；拖动走 `startSystemMove()`（保留贴边、拖回还原等系统行为），双击标题栏切换最大化，右键标题栏弹出还原 / 最小化 / 最大化 / 关闭菜单；按钮区域用 `rightMargin` 排除在拖动热区之外。⑥**范围**：主窗口 `visible: false`，由 `main.cpp` 在 `WindowChrome::attach()` 之后再显示，避免第一帧闪出系统标题栏，也不会因框架边距归零而发生尺寸跳变（`attach()` 里先记录框架位置与客户区尺寸再恢复）。⑦**窗口状态保存**：最大化 / 最小化时的框架坐标（含 `-32000`）不再写入 `ui.ini`，只保存还原后的尺寸与位置。⑧**启动不描边**：卡片网格仍然持有焦点（Enter / 空格可直接复制当前卡片），但新增 `keyboardNavigation` 标志——只有方向键 / Home / End / PageUp / PageDown 移动过位置后才给当前卡片描边，鼠标点击或焦点离开即清除；新建弹窗同理（`keyboardFocusVisible`：鼠标点开时不显示类型磁贴的焦点描边，Tab / 方向键后才显示，磁贴仍保留焦点以便直接回车）。⑨**标题栏按钮尺寸**：改用 Edge 的按钮几何——46 DIP 宽、占满标题栏高度、无圆角悬停底色（关闭按钮整块变红、图标转白），图标 14 DIP（约为原 20 DIP 的 70%）。`CaptionButton` 从 `Button` 改为普通 `Item` + `HoverHandler` / `TapHandler`，避免 `Button` 把 `contentItem` 拉伸到整个内边距区域导致图标尺寸不可控。`tst_ui_flows` 新增标题栏断言（标题栏贴顶、按钮顺序与齐右、按钮不取焦点、最大化后窗口不越出工作区、图标随状态切换、还原）与启动描边断言（初始无描边、悬停有、移开消失、方向键出现在当前卡片、鼠标点击清除、新建弹窗鼠标打开无描边 / Tab 后出现），Debug 与 Release 各 9 组测试全部通过。

- 2026-10-20 滚动回弹移除：话术卡片列表（`GridView`）设置 `boundsBehavior: Flickable.StopAtBounds`，滑到最顶或最底时直接停住，不再有回弹动画（与分类栏行为一致）。
- 2026-10-19 卡片边距修复：①**拖拽边框被裁切**——卡片网格原本 `clip: true` 且卡片恰好占满单元格，外边距为 0，拖拽时 2px 描边有一半被裁掉（左侧与第一排顶部），现为网格增加 4 DIP 内边距（`leftMargin` / `rightMargin` / `topMargin`），描边完整可见；②**左右留白不等**——列宽取整后右侧多出的像素现由最后一列吸收（`lastColumnExtra`），左右外边距严格相等（实测左右各 4 DIP）；③列数判断同步按去掉内边距后的可用宽度计算，窄面板仍为双列。`tst_ui_flows` 新增卡片边距断言（四边留白 ≥ 描边宽度、左右留白相等），测试 9 组 Debug/Release 全通过。
- 2026-10-19 分类栏样式统一：分类标签（`CategoryChip.qml`）改为与底部类型筛选按钮（`TypeFilterButton.qml`）完全一致的样式与动画——高度 30 DIP、圆角 9、选中白底 + 1px 描边 + 蓝色文字、未选中透明底 + 悬停浅蓝底（同为透明的过渡色，避免灰闪），文字 12px，`ColorAnimation` 140ms；保留各分类的彩色圆点。分类栏容器高度同步为 30 DIP、标签间距 6，与筛选栏一致。`tst_ui_flows` 13 项全通过，9 组测试 Debug/Release 通过。
- 2026-10-19 回退无边框方案：按要求**完整移除**无边框自绘窗口与"固定宽度模式"——删除 `FrameController`（C++）、`TitleBar.qml`、QML 投影层、`app.svg` / `panel-*.svg`、`ui.ini` 中的 `fixedWidth` 字段及其读写接口，窗口恢复为 Windows 原生边框 + 系统投影（标题栏拖动、双击最大化、Aero 贴边均由系统提供）。原因：无边框方案既无法消除横向缩放黑边，又带来 QML 自绘投影（非系统阴影）与标题栏按钮悬停时的闪黑问题。保留本次的其他改进：底部类型筛选栏（计数左对齐 / 按钮右对齐）、卡片列表与分类标签的间距、卡片 96 DIP 紧凑高度、点击弹窗外关闭等。`tst_ui_flows` 移除无边框与固定宽度断言，保留底部栏左右分列断言，测试 9 组 Debug/Release 全通过。
- 2026-10-19 外观改版与可选固定宽度：①**固定宽度模式**——工具栏新增"窄面板"开关，开启后面板宽度锁定（420 DIP）、仅可上下缩放，横向缩放因此彻底不会发生，松开后自动收回到固定宽度，开关状态持久化到 `ui.ini`；开启时左右缩放热区与横向缩放请求一并禁用。②**窗口外观**参考 Clash Party 重做：无边框窗口改为圆角卡片 + 1px 描边 + **QML 自绘柔和投影**（不依赖原生窗口也有阴影），标题栏改为浅色横条（左侧应用图标 + "TalkBOX 话术库"，右侧最小化 / 最大化 / 关闭，关闭按钮悬停变红），内容区与标题栏用浅色底分隔；新增 `TitleBar.qml`、`app.svg`、`panel-*.svg`。③固定宽度为首选方案，因为**横向缩放黑边**经实测在无边框方案下同样存在（条带宽度恰等于横向放大量，属 Qt Quick 场景图的平台限制），固定宽度可完全规避。`tst_ui_flows` 新增固定宽度模式断言（宽度锁定、高度仍可调、状态持久化），测试 9 组 Debug/Release 全通过。
- 2026-10-19 界面细化与无边框窗口：①**底部筛选栏排版**——结果计数左对齐、全部内容 / 图片 / PDF 按钮右对齐，两者分居两端（不再挤在一起）；②**列表间距**——卡片列表上方增加 14 DIP 间距，第一排卡片不再紧贴分类标签；③**无边框自绘窗口**——移除系统标题栏，改由 QML 绘制标题栏（应用图标 + 标题 + 最小化 / 最大化 / 关闭）和四边四角缩放热区，拖动与缩放通过 `startSystemMove` / `startSystemResize` 交给系统处理，Aero 贴边、双击最大化、多屏行为保持不变；新增 `FrameController`（C++，核心库）+ `TitleBar.qml`，保存的窗口位置改为窗口可见后应用（无边框窗口由平台先行显示，会覆盖此前设置的位置）。关于**缩放黑边**：已实测无边框方案**同样无法消除**该条带，现象与窗口边框无关，属 Qt Quick 场景图在 Windows 上逐列重绘的平台限制，此处不再宣称修复。`tst_ui_flows` 新增底部栏左右对齐与无边框标题栏断言，测试 9 组 Debug/Release 全通过。
- 2026-10-19 布局与细节（按用户要求）：①**类型筛选移到底部**——全部内容 / 图片 / PDF 三个按钮从列表上方移到窗口最底部（列表下方，右侧对齐），列表区域完整用于卡片，顶部只保留分类筛选与内容计数。②**卡片更紧凑**——卡片高度 118 → 96 DIP，内边距 10 → 8/9、分类标签 22 → 20、附件缩略图 52 → 48，文字卡片底部不再留大片空白。③**缩放黑边如实说明**——已用截图逐帧量化确认：窗口向左/向下放大后，新露出的条带宽度恰好等于放大尺寸（29 / 66 / 104 px）且**持续存在**，与 QML 内容无关（空场景同样复现），属 Qt Quick 场景图在 Windows 上的已知行为（QTBUG-46074 一类）。已保留整窗不透明页面底色层并在尺寸变化时请求重绘以尽量减轻，但未能彻底消除，此处如实记录而不宣称已修复。`tst_ui_flows` 新增"类型筛选位于窗口底部且不与列表重叠"断言，测试 9 组 Debug/Release 全通过。
- 2026-10-19 交互体验（按用户要求）：①**弹窗点击外部关闭**——所有弹窗（新建 / 编辑 / 重命名 / 修改分类 / 删除确认 / 图片预览 / 设置）改为 `CloseOnEscape | CloseOnPressOutside`，点击软件内面板外任意空白处或 Esc 即可关闭；有未保存内容时不会静默丢弃，而是自动转为"放弃已填写内容？"确认（继续编辑 / 放弃），导入进行中或备份 / 恢复进行中则不会被误关闭。说明：模态 Popup 按 Qt 设计会吞掉遮罩上的鼠标事件，因此关闭逻辑走 `closePolicy` 而非遮罩里的 `MouseArea`（`DialogScrim` 现在只负责视觉遮罩）。②**窗口缩放黑边修复**——整窗增加不透明页面底色层（`#F3F5F9`），并在宽高变化时立即 `requestUpdate()`，横向 / 竖向放大时右侧 / 底部不再出现未绘制的黑色阴影。③**拖拽内容反馈**——拖拽时卡片浮起（2px 主色描边 + 1.03 缩放 + 内容淡化）并显示复制光标，窗口底部浮条提示"松开即可把文字拖到聊天窗口 / 松开即可把文件拖到聊天窗口"同时显示被拖内容摘要，拖拽结束自动隐藏。`tst_ui_flows` 新增点击外部关闭（含嵌套弹窗与未保存内容）、拖拽反馈断言，测试 9 组 Debug/Release 全通过。

- 2026-10-19 交互调整（按用户要求）：**卡片主体点击一律"直接复制"**（文本=全文、图片=位图、PDF=文件引用，可 Ctrl+V 粘贴到微信/资源管理器）；预览、打开等操作改为**右键或右上角「…」**弹出菜单，菜单新增"复制图片 / 复制文件"并按类型显示；新增 `SystemService.copyFileToClipboard()`（文件引用写剪贴板，带瞬时占用重试）。修复**图片预览弹窗显示异常**：此前弹窗固定 660×420，紧凑面板（≤420 DIP）下按钮整行溢出窗口、关闭按钮被裁掉——现宽高与预览区高度均随面板自适应（含窄窗口下的按钮换行），关闭按钮移到标题行右上角，Esc 关闭；`tst_ui_flows` 增加点击复制（三类型）、右键菜单、弹窗不裁切等断言，`tst_system_clipboard` 增加文件引用复制用例（测试 9 组 Debug/Release 全通过）。
- 2026-10-18 图标体系：全部图标改为 **Material Symbols**（Apache-2.0，PDF 使用专用 `picture_as_pdf`），置顶图标改为 **Bootstrap Icons `pin-angle`**（MIT，经典斜向图钉，与齿轮协调）；所有图标显式声明 `fillMode: PreserveAspectFit` 保证原始比例；许可文件见 `LICENSES/`。
- 2026-10-18 界面细节：工具栏整体高度 40 → 34 DIP（搜索框 / 新建 / 置顶 / 设置）；置顶与设置改为无边框图标按钮（悬停浅蓝圆角底、置顶时图标变蓝），替换原先的白底描边方块按钮。
- 2026-10-18 体验修复：恢复标准窗口标题栏（最小化 / 最大化 / 关闭，此前 `flags` 漏了 `WindowTitleHint`）；图标全部改用 Lucide 图标库（ISC，附许可证）；紧凑面板默认双列（≥360 DIP 双列，卡片内边距收紧）。
- 2026-10-18 辅助面板体验：默认窗口改为紧凑侧边面板（420 DIP，首次停靠右侧）；卡片列数随宽度自适应（单 / 双 / 三列）；新增**卡片拖拽**（文本拖入聊天窗口直接粘贴、图片 / PDF 以文件拖出）；新增**置顶开关**；记住窗口大小 / 位置 / 置顶状态；弹窗宽度自适应窄窗口。测试 9 组 100% 通过。
- 2026-10-18 阶段 C：自定义分类管理（新建 / 重命名 / 删除并迁移内容）、`.talkboxbackup` 备份与恢复（清单校验、路径穿越防护、暂存校验后切换、失败自动回滚、自动安全副本）、设置页（数据备份 / 分类管理 / 关于）、日志轮换 5 代并记录迁移与备份恢复事件；测试扩展到 9 组（Debug/Release 全通过）。
- 2026-10-17 阶段 B（验收修复）：独立发布目录 `dist\TalkBOX`（windeployqt，双击即用）；修复附件保存时 QML 引用错误（`displayNameField`）；修复菜单宽度塌陷与隐藏项占位；修复卡片网格单列（右侧大片空白）；修复分类 / 类型筛选按钮悬停灰闪；剪贴板写入增加瞬态占用重试；发布版全流程实测通过（图片 / PDF 导入、预览复制、系统默认程序打开、菜单、重启保留）。
- 2026-10-17 阶段 B（续）：新增 `tst_ui_flows` UI 集成测试（真实 QML + 合成事件，覆盖 AT-04/08/11）；主窗口尺寸按桌面可用区自适应。
- 2026-10-17 阶段 B：图片 / PDF 附件闭环（异步导入、缩略图、预览复制、系统打开）、三点菜单与编辑 / 重命名 / 改分类 / 删除确认、异常与空状态；测试扩展到 6 组（Debug/Release 全通过）。
- 2026-10-17 阶段 A：工程骨架、SQLite 数据闭环、双列 UI、搜索 / 筛选、复制、测试（Debug/Release 全通过）。
