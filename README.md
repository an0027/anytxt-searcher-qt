# AnyTXT Searcher (C++/Qt5)

高性能桌面全文搜索工具，基于 Qt5 + Xapian 构建。支持中文分词、多种文档格式解析。

## 功能特性

- 🔍 **全文搜索** — 极速中文搜索引擎，支持模糊匹配、拼写纠错
- 📄 **多格式解析** — TXT、PDF、DOCX、图像OCR
- 🎨 **GUI界面** — Qt5 Widgets 原生界面，搜索/结果/预览/筛选一体化
- ⚡ **高性能** — C++ 原生实现，索引速度 4000+ docs/sec，搜索 <1ms

## 系统要求

- Linux x86_64
- Qt5 (Widgets + Concurrent)
- Xapian 1.4+
- Poppler-Qt5 (PDF支持)
- Tesseract + Leptonica (OCR支持)

## 编译

```bash
mkdir build && cd build
cmake ..
make -j$(nproc)
```

## 使用

```bash
./build/anytxt-searcher
# 指定索引目录
./build/anytxt-searcher -i /path/to/index
```

## 性能

| 指标 | 数值 |
|------|------|
| 索引速度 | ~4,188 docs/sec |
| 搜索延迟 | <1ms |
| 60万文件索引预估 | ~2.4分钟 |
| 支持文档数 | 无上限 |
