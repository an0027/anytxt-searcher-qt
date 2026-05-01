#ifndef CORE_DOCUMENT_H
#define CORE_DOCUMENT_H

#include <QString>
#include <QMap>
#include <QVector>
#include <cstdint>

struct Document {
    int64_t docId = -1;
    int percent = 0;
    double relevance = 0.0;
    int rank = 0;
    QString filePath;
    QString fileName;
    QString fileExt;
    QString title;
    QString content;
    int64_t fileSize = 0;
    int64_t modifiedTime = 0;
    QString mimeType;
    QMap<QString, QString> metadata;
};

#endif // CORE_DOCUMENT_H
