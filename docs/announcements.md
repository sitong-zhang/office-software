# 公告机制

公告是无需发布新版本即可送达的消息。它们在启动时从本仓库读取，每条只展示一次，点击**继续**后进入应用。

## 数据来源

```text
announcements.json          （仓库根目录，main 分支）
```

客户端读取原始文件：

```text
https://raw.githubusercontent.com/sitong-zhang/office-software/main/announcements.json?t=<unix-ms>
```

其中 `t` 参数用于绕过缓存；不加该参数时，CDN 可能返回几分钟前的旧内容。

## 格式

```json
{
  "schema_version": 1,
  "updated_at": "2026-11-15T12:00:00+08:00",
  "announcements": [
    {
      "id": "2026-11-15-project-start",
      "title": "Chenyi Office项目启动",
      "body": "第一段。\n\n第二段。",
      "severity": "info",
      "published_at": "2026-11-15T12:00:00+08:00",
      "expires_at": null,
      "min_version": null,
      "max_version": null,
      "action_label": "查看路线图",
      "action_url": "https://github.com/sitong-zhang/office-software/blob/main/docs/roadmap.md"
    }
  ]
}
```

| 字段 | 必填 | 含义 |
| --- | --- | --- |
| `schema_version` | 是 | 当前为 `1` |
| `updated_at` | 是 | 最后编辑时间，ISO 8601 带时区 |
| `announcements[].id` | 是 | 稳定唯一标识；不要复用或修改，因为客户端会记住已读记录 |
| `title` | 是 | 简短标题，纯文本 |
| `body` | 是 | 纯文本，`\n` 换行，建议控制在 2000 字以内 |
| `severity` | 是 | `info` 或 `important`，仅影响强调程度 |
| `published_at` | 是 | 该时间之前不展示 |
| `expires_at` | 否 | 该时间之后隐藏；`null` 表示不过期 |
| `min_version` / `max_version` | 否 | 适用的客户端版本范围；`null` 表示不限 |
| `action_label` / `action_url` | 否 | 可选链接，在浏览器中打开；不会替代**继续**按钮 |

## 客户端行为

1. 启动先进行更新检查（见[更新机制](updater.md)）。
2. 即使存在可用更新，也会获取公告，因此推迟更新不会隐藏版本通知。
3. 获取公告的超时为 10 秒，最多尝试 2 次。
4. 按发布时间、过期时间、客户端版本范围和本地已读记录进行过滤。
5. 若仍有公告，全屏公告页按发布时间顺序展示。**继续**是唯一操作，没有跳过链接。
6. 点击**继续**会在本地记录已读的公告标识并打开应用。每条公告在每次安装中只展示一次。
7. 若获取失败，应用正常打开，下次启动再试。网络故障不会阻塞启动。
8. 已读记录按用户配置保存在本地，升级后仍然保留。

## 内容规范

- 不包含凭据、个人信息，或指向本仓库发布页与文档之外的链接。
- `important` 仅用于版本发布、安全通知和影响使用的变更。
- 过期公告可在 90 天后移除，但至少保留最近一条。
- 公告不替代发行说明：版本相关的细节应放在版本发布页。
