# 从旧版（PySide6）迁移数据

TalkBOX 1.0 是 Qt 重写版，数据存储从「JSON 文件 + 图片目录」换成了 SQLite。
两者的格式完全不同，无法直接读取，需要一个转换步骤。

旧版数据位置（PySide6 版本）：

```text
%APPDATA%\Talkbox\
├─ talk_data.json     # 话术列表（文本 / 图片 / PDF 各一条记录）
├─ images\            # 图片原文件
└─ pdfs\<uuid>\       # PDF 原文件
```

新版数据位置：

```text
%LOCALAPPDATA%\TalkBOX\
├─ talkbox.db
└─ attachments\
```

## 迁移方式

1. **先备份**新版的 `%LOCALAPPDATA%\TalkBOX`（如果已经用起来了），并确保 TalkBOX 已退出。
2. 用下面任一方式转换：
   - 如果你保留了旧版导出文件，把它们交给维护者按需转换；
   - 或自行按 `talk_data.json` 的字段映射写入 SQLite（见下方「字段映射」）。
3. 启动新版，确认「全部」里的条数与分类都正确。

> 旧版里的 `pinned`（收藏）字段在新版没有对应功能（新版「置顶」是窗口级开关），不会被迁移。

## 字段映射

| 旧 JSON 字段 | 新版存放位置 | 说明 |
|---|---|---|
| `content` | `snippets.content`（`type='text'`） | 去除首尾空白 |
| `category` | `categories.name` → `snippets.category_id` | 分类不存在时自动新建（UUID 主键） |
| `type: "image"` + `image` | `snippets.relative_path` + `attachments\<uuid>.<ext>` | 文件复制进附件目录，按内容校验，避免重复 |
| `type: "pdf"` + `pdf` | 同上 | MIME 记为 `application/pdf` |
| 记录顺序 | `created_at` / `updated_at` | 新版按 `updated_at DESC` 排序，导入时按旧顺序倒序写时间戳即可保持原顺序 |

附件显示名（`display_name`）建议用旧版原始文件名（例如 `交流充电桩价格表.png`），
而不是存储用的 UUID 名，这样卡片上看到的就是你熟悉的名称。

## 已知边界

- 旧版导出的 JSON 如果指向了已经被删除的图片 / PDF，转换时会跳过并报告缺失条目，不会产生空记录。
- 同一个文件被多条记录引用时，新版会各自复制一份（新版按记录管理附件）。
- 迁移完成后，旧目录 `%APPDATA%\Talkbox` 可以保留一段时间再删除；新版不会读写它。
