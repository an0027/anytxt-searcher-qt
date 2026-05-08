# AnyTXT Searcher 文档索引

> 高性能桌面全文搜索工具

---

## 文档目录

| 文档 | 说明 | 链接 |
|------|------|------|
| **使用手册** | 完整的中文使用手册，包含安装、搜索、格式支持、索引管理、FAQ 等 | [user_guide.md](user_guide.md) |
| **README** | 项目概览、编译说明、依赖要求 | [README.md](/README.md) |

## 源文件结构

```
anytxt-searcher-qt/
├── CMakeLists.txt              # 主构建文件
├── README.md                   # 项目说明
├── src/
│   ├── main.cpp                # 入口点
│   ├── core/                   # 核心功能
│   │   ├── document_processor.h  # 文档处理器基类
│   │   ├── xapian_database.cpp   # Xapian 数据库封装
│   │   ├── xapian_indexer.cpp    # 索引器
│   │   ├── xapian_searcher.cpp   # 搜索器
│   │   ├── config.cpp            # 配置文件管理
│   │   ├── file_watcher.cpp      # 文件监控
│   │   ├── index_queue.cpp       # 索引队列
│   │   ├── rule_engine.cpp       # 规则引擎
│   │   ├── notification_manager.cpp # 通知管理
│   │   └── theme_config.cpp      # 主题配置
│   ├── parser/                 # 文件解析器
│   │   ├── text_parser.cpp     # 纯文本/代码文件解析器
│   │   ├── pdf_parser.cpp      # PDF 解析器 (poppler)
│   │   ├── docx_parser.cpp     # DOCX/DOCM 解析器 (libzip)
│   │   ├── xlsx_parser.cpp     # XLSX/XLSM 解析器 (libzip)
│   │   ├── pptx_parser.cpp     # PPTX/PPTM 解析器 (libzip)
│   │   ├── epub_parser.cpp     # EPUB 电子书解析器 (libzip)
│   │   ├── eml_parser.cpp      # EML 邮件解析器
│   │   ├── rtf_parser.cpp      # RTF 富文本解析器
│   │   ├── doc_parser.cpp      # DOC 老版Word解析器
│   │   ├── wps_parser.cpp      # WPS/ET/DPS 解析器
│   │   └── parser_manager.cpp  # 解析器管理器
│   ├── gui/                    # 图形界面
│   │   ├── main_window.cpp     # 主窗口
│   │   ├── search_bar.cpp      # 搜索栏
│   │   ├── results_widget.cpp  # 结果列表
│   │   ├── preview_widget.cpp  # 预览区域
│   │   ├── file_panel.cpp      # 文件浏览面板
│   │   └── match_panel.cpp     # 命中段落面板
│   ├── dialogs/                # 对话框
│   │   ├── help_dialog.cpp     # 使用手册对话框
│   │   ├── about_dialog.cpp    # 关于对话框
│   │   ├── import_dialog.cpp   # 导入对话框
│   │   ├── export_dialog.cpp   # 导出对话框
│   │   ├── preferences_dialog.cpp # 偏好设置
│   │   ├── theme_dialog.cpp    # 主题设置
│   │   └── watch_settings_dialog.cpp # 监控设置
│   └── utils/                  # 工具函数
│       ├── file_utils.cpp
│       └── string_utils.cpp
├── docs/
│   ├── index.md                # 文档索引（本文件）
│   └── user_guide.md           # 完整使用手册
└── installer/
    └── anytxt-searcher.wxs     # 安装程序配置
```

## 解析器一览

| 解析器 | 格式 | 依赖 | 状态 |
|--------|------|------|------|
| TextParser | TXT/MD/CSV/代码等 | 无 | ✅ 默认启用 |
| PdfParser | PDF | poppler-cpp | ⚠️ 可选 |
| DocxParser | DOCX/DOCM | libzip | ⚠️ 可选 |
| XlsxParser | XLSX/XLSM | libzip | ⚠️ 可选 |
| PptxParser | PPTX/PPTM | libzip | ⚠️ 可选 |
| EpubParser | EPUB | libzip | ⚠️ 可选 |
| EmlParser | EML | 无 | ✅ 默认启用 |
| RtfParser | RTF | 无 | ✅ 默认启用 |
| DocParser | DOC | libzip | ⚠️ 可选 |
| WpsParser | WPS/ET/DPS | libzip | ⚠️ 可选 |

---

*本页面由系统自动维护*
