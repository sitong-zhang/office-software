# Chenyi Office

**一套面向 Windows 10 / 11 64 位的办公软件，拥有统一的界面设计、专属启动动画、可选更新和应用内公告。**

[![许可证：MPL 2.0](https://img.shields.io/badge/license-MPL%202.0-1f5c46.svg)](LICENSE)
[![平台：Windows x64](https://img.shields.io/badge/platform-Windows%2010%2F11%20x64-0078D4.svg)](https://sitong-zhang.github.io/office-software/)
[![版本](https://img.shields.io/badge/version-0.4.4-1f5c46.svg)](https://github.com/sitong-zhang/office-software/releases/latest)

Chenyi Office 提供文字文档、电子表格、演示文稿、绘图和公式等常用办公能力，并把启动、界面、更新和公告整合为一致的体验。

## 下载与安装

- **安装包**：[ChenyiOffice-0.4.4-x64.exe](https://github.com/sitong-zhang/office-software/releases/latest)
- **适用平台**：Windows 10 / Windows 11，64 位
- **安装说明**：首次安装需要发布者提供的安装密码，更新时无需再次输入。

安装包、校验值与公开证书统一在 [Releases](https://github.com/sitong-zhang/office-software/releases) 发布。建议只从本项目官方发布页下载。

## 产品特性

- **统一界面**：启动中心、编辑界面、菜单、对话框与设置使用一致的设计语言；文字文档、电子表格、演示文稿和绘图默认采用三段式标签页，界面模式可随时切换，不会在安装后被锁死。
- **专属启动动画**：品牌开屏兼具启动与更新检查，动画结束即进入工作状态。
- **可选更新**：每次启动检查新版本，发现更新时可以选择立即安装、稍后提醒或直接关闭；在作出选择前，当前版本始终可以正常使用。详见[更新机制](docs/updater.md)。
- **应用内公告**：版本发布与维护通知在启动时展示一次，不打断正常使用。详见[公告机制](docs/announcements.md)。
- **中文体验**：界面、菜单、启动中心与模块名称均为中文。

## 项目文档

| 文档 | 说明 |
| --- | --- |
| [项目主页](https://sitong-zhang.github.io/office-software/) | 产品介绍、下载入口与更新策略 |
| [路线图](docs/roadmap.md) | 各阶段目标与当前进度 |
| [更新机制](docs/updater.md) | 版本检查、下载校验与安装流程 |
| [公告机制](docs/announcements.md) | 应用内公告的格式与展示规则 |
| [更新日志](CHANGELOG.md) | 每个版本面向用户的变更 |
| [安全政策](SECURITY.md) | 支持的版本与安全漏洞报告方式 |

## 参与贡献

欢迎通过 [Issues](https://github.com/sitong-zhang/office-software/issues) 反馈问题、提交功能建议，或通过 Pull Request 改进文档与代码。参与前请先阅读[贡献指南](CONTRIBUTING.md)与[行为准则](CODE_OF_CONDUCT.md)。

## 许可证

本项目源代码采用 [Mozilla Public License 2.0](LICENSE) 授权。产品图标使用 Lucide（ISC）与 Material Design Icons（Apache 2.0）等开源资源，对应的许可证文件随资源一并保存在 `app/branding/third-party/` 目录中；启动器解析本地配置所用的 [pugixml](https://pugixml.org/)（MIT）位于 `app/third_party/pugixml/`，许可证与其源码一同提供。

## 安全

更新仅接受由本项目固定签名证书签署、且 SHA-256 与发布清单一致的安装包。若需报告安全漏洞，请按[安全政策](SECURITY.md)中的私密渠道提交，不要通过公开 Issue 披露细节。
