/*
 * rule_engine.cpp - 搜索规则引擎实现

实现将 SearchRule 转换为 Xapian 查询字符串的逻辑，
包括文本查询构建、数值范围查询、日期范围查询、
高级查询字符串解析和字段前缀映射。
 */

#include "rule_engine.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>
#include <QRegularExpression>
#include <QDebug>
#include <cmath>

// ===== RuleCondition =====

QJsonObject RuleCondition::toJson() const
{
    QJsonObject obj;
    obj["field"] = static_cast<int>(field);
    obj["operator"] = static_cast<int>(op);
    obj["value"] = value;
    obj["negate"] = negate;

    QJsonArray arr;
    for (const auto& v : valueList) arr.append(v);
    obj["valueList"] = arr;
    return obj;
}

RuleCondition RuleCondition::fromJson(const QJsonObject& obj)
{
    RuleCondition c;
    c.field = static_cast<RuleField>(obj["field"].toInt(0));
    c.op = static_cast<RuleOp>(obj["operator"].toInt(0));
    c.value = obj["value"].toString();
    c.negate = obj["negate"].toBool(false);
    for (const auto& v : obj["valueList"].toArray())
        c.valueList.append(v.toString());
    return c;
}

// ===== SearchRule =====

SearchRule::SearchRule(const QString& name, const QString& desc, RuleOp op)
    : m_name(name), m_description(desc), m_combineOp(op)
{
}

SearchRule& SearchRule::addCondition(const RuleCondition& condition)
{
    m_conditions.append(condition);
    return *this;
}

SearchRule& SearchRule::addCondition(RuleField field, RuleOp op,
                                     const QString& value, bool negate)
{
    RuleCondition c;
    c.field = field;
    c.op = op;
    c.value = value;
    c.negate = negate;
    m_conditions.append(c);
    return *this;
}

SearchRule& SearchRule::addCondition(RuleField field, RuleOp op,
                                     const QStringList& values, bool negate)
{
    RuleCondition c;
    c.field = field;
    c.op = op;
    c.valueList = values;
    if (!values.isEmpty()) c.value = values.first();
    c.negate = negate;
    m_conditions.append(c);
    return *this;
}

QJsonObject SearchRule::toJson() const
{
    QJsonObject obj;
    obj["name"] = m_name;
    obj["description"] = m_description;
    obj["operator"] = static_cast<int>(m_combineOp);

    QJsonArray arr;
    for (const auto& c : m_conditions) arr.append(c.toJson());
    obj["conditions"] = arr;
    return obj;
}

SearchRule SearchRule::fromJson(const QJsonObject& obj)
{
    SearchRule rule(
        obj["name"].toString(),
        obj["description"].toString(),
        static_cast<RuleOp>(obj["operator"].toInt(0))
    );

    for (const auto& v : obj["conditions"].toArray()) {
        rule.m_conditions.append(RuleCondition::fromJson(v.toObject()));
    }
    return rule;
}

QString SearchRule::toJsonString() const
{
    QJsonDocument doc(toJson());
    return QString::fromUtf8(doc.toJson(QJsonDocument::Compact));
}

SearchRule SearchRule::fromJsonString(const QString& json)
{
    QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8());
    return fromJson(doc.object());
}

// ===== RuleEngine =====

RuleEngine::RuleEngine()
{
    // Build prefix cache
    m_prefixCache["content"] = "";
    m_prefixCache["title"] = "S";
    m_prefixCache["file"] = "F";
    m_prefixCache["filename"] = "F";
    m_prefixCache["type"] = "XTYPE";
    m_prefixCache["ext"] = "XEXT";
    m_prefixCache["file_type"] = "XTYPE";
    m_prefixCache["file_extension"] = "XEXT";
    m_prefixCache["author"] = "XAUTHOR";
    m_prefixCache["keywords"] = "XKEYWORDS";
    m_prefixCache["mime_type"] = "XMIME";
}

QString RuleEngine::fieldPrefix(RuleField field)
{
    switch (field) {
        case RuleField::TITLE: return "S";
        case RuleField::FILENAME: return "F";
        case RuleField::FILE_PATH: return "P";
        case RuleField::FILE_TYPE: return "XTYPE";
        case RuleField::FILE_EXTENSION: return "XEXT";
        case RuleField::AUTHOR: return "XAUTHOR";
        case RuleField::KEYWORDS: return "XKEYWORDS";
        case RuleField::MIME_TYPE: return "XMIME";
        default: return "";
    }
}

QString RuleEngine::fieldToPrefix(const QString& field) const
{
    return m_prefixCache.value(field.toLower(), "");
}

QString RuleEngine::fieldDisplayName(RuleField field)
{
    switch (field) {
        case RuleField::CONTENT: return "内容";
        case RuleField::TITLE: return "标题";
        case RuleField::FILENAME: return "文件名";
        case RuleField::FILE_PATH: return "文件路径";
        case RuleField::FILE_TYPE: return "文件类型";
        case RuleField::FILE_EXTENSION: return "扩展名";
        case RuleField::FILE_SIZE: return "文件大小";
        case RuleField::MODIFIED_DATE: return "修改日期";
        case RuleField::CREATED_DATE: return "创建日期";
        case RuleField::AUTHOR: return "作者";
        case RuleField::KEYWORDS: return "关键词";
        case RuleField::MIME_TYPE: return "MIME类型";
        default: return "未知";
    }
}

QString RuleEngine::fieldKey(RuleField field)
{
    switch (field) {
        case RuleField::CONTENT: return "content";
        case RuleField::TITLE: return "title";
        case RuleField::FILENAME: return "filename";
        case RuleField::FILE_PATH: return "file_path";
        case RuleField::FILE_TYPE: return "file_type";
        case RuleField::FILE_EXTENSION: return "file_extension";
        case RuleField::FILE_SIZE: return "file_size";
        case RuleField::MODIFIED_DATE: return "modified_date";
        case RuleField::CREATED_DATE: return "created_date";
        case RuleField::AUTHOR: return "author";
        case RuleField::KEYWORDS: return "keywords";
        case RuleField::MIME_TYPE: return "mime_type";
        default: return "content";
    }
}

QString RuleEngine::opDisplayName(RuleOp op)
{
    switch (op) {
        case RuleOp::CONTAINS: return "包含";
        case RuleOp::EQUALS: return "等于";
        case RuleOp::STARTS_WITH: return "开头是";
        case RuleOp::ENDS_WITH: return "结尾是";
        case RuleOp::GREATER_THAN: return "大于";
        case RuleOp::LESS_THAN: return "小于";
        case RuleOp::BETWEEN: return "介于";
        case RuleOp::IN: return "在列表中";
        case RuleOp::REGEX: return "正则匹配";
        case RuleOp::AND: return "与";
        case RuleOp::OR: return "或";
        case RuleOp::NOT: return "非";
        default: return "未知";
    }
}

bool RuleEngine::isNumericField(RuleField field)
{
    return field == RuleField::FILE_SIZE;
}

bool RuleEngine::isDateField(RuleField field)
{
    return field == RuleField::MODIFIED_DATE ||
           field == RuleField::CREATED_DATE;
}

QVector<QPair<RuleField, QString>> RuleEngine::supportedFields()
{
    return {
        {RuleField::CONTENT, "内容"},
        {RuleField::TITLE, "标题"},
        {RuleField::FILENAME, "文件名"},
        {RuleField::FILE_PATH, "文件路径"},
        {RuleField::FILE_TYPE, "类型"},
        {RuleField::FILE_EXTENSION, "扩展名"},
        {RuleField::FILE_SIZE, "大小"},
        {RuleField::MODIFIED_DATE, "修改日期"},
        {RuleField::AUTHOR, "作者"}
    };
}

QString RuleEngine::escapeValue(const QString& value) const
{
    QString v = value;
    // Escape special Xapian characters: ( ) { } [ ] ^ ~ * :
    v.replace("\\", "\\\\");
    v.replace("\"", "\\\"");
    v.replace("(", "\\(");
    v.replace(")", "\\)");
    v.replace("{", "\\{");
    v.replace("}", "\\}");
    v.replace("[", "\\[");
    v.replace("]", "\\]");
    v.replace("^", "\\^");
    v.replace("~", "\\~");
    v.replace("*", "\\*");
    v.replace(":", "\\:");
    return v;
}

QString RuleEngine::formatFileSize(int64_t bytes)
{
    if (bytes < 1024) return QString::number(bytes) + " B";
    if (bytes < 1024 * 1024) return QString::number(bytes / 1024.0, 'f', 1) + " KB";
    if (bytes < 1024LL * 1024 * 1024)
        return QString::number(bytes / (1024.0 * 1024.0), 'f', 1) + " MB";
    return QString::number(bytes / (1024.0 * 1024.0 * 1024.0), 'f', 2) + " GB";
}

QString RuleEngine::prettyValue(const QString& field, const QString& value)
{
    if (field == "file_size") {
        return formatFileSize(value.toLongLong());
    }
    return value;
}

QString RuleEngine::parseRule(const SearchRule& rule) const
{
    if (!rule.isValid()) return "";

    QStringList parts;
    for (const auto& cond : rule.conditions()) {
        QString q = buildConditionQuery(cond);
        if (!q.isEmpty()) parts.append(q);
    }

    if (parts.isEmpty()) return "";

    if (parts.size() == 1) return parts.first();

    QString joinOp = (rule.combineOp() == RuleOp::OR) ? " OR " : " AND ";
    QString result = parts.join(joinOp);
    result.prepend("(");
    result.append(")");
    return result;
}

QString RuleEngine::buildConditionQuery(const RuleCondition& cond) const
{
    RuleField field = cond.field;
    RuleOp op = cond.op;
    QString value = cond.value;
    QStringList values = cond.valueList;

    if (value.isEmpty() && op != RuleOp::AND && op != RuleOp::OR && op != RuleOp::NOT) {
        return "";
    }

    QString result;
    if (isNumericField(field)) {
        result = buildNumericQuery(fieldKey(field), op, value, values);
    } else if (isDateField(field)) {
        result = buildDateQuery(fieldKey(field), op, value, values);
    } else {
        result = buildTextQuery(fieldKey(field), op, value);
    }

    if (cond.negate && !result.isEmpty()) {
        result = "NOT (" + result + ")";
    }

    return result;
}

QString RuleEngine::buildTextQuery(const QString& field, RuleOp op,
                                    const QString& value) const
{
    QString prefix = fieldToPrefix(field);
    QString escaped = escapeValue(value);

    switch (op) {
        case RuleOp::CONTAINS:
        case RuleOp::EQUALS:
            if (!prefix.isEmpty())
                return field + ":" + escaped;
            return escaped;

        case RuleOp::STARTS_WITH:
            if (!prefix.isEmpty())
                return field + ":" + escaped + "*";
            return escaped + "*";

        case RuleOp::ENDS_WITH:
            if (!prefix.isEmpty())
                return field + ":" + escaped;
            // Xapian doesn't support suffix wildcard natively
            return escaped;

        case RuleOp::REGEX:
            // Approximate regex as wildcard for Xapian
            {
                QString rx = value;
                rx.replace(".*", "*").replace(".", "?");
                if (!prefix.isEmpty())
                    return field + ":" + rx;
                return rx;
            }

        case RuleOp::IN:
            {
                QStringList parts;
                for (const auto& v : value.split(",", Qt::SkipEmptyParts)) {
                    if (!prefix.isEmpty())
                        parts.append(field + ":" + v.trimmed());
                    else
                        parts.append(v.trimmed());
                }
                if (parts.isEmpty()) return "";
                if (parts.size() == 1) return parts.first();
                return "(" + parts.join(" OR ") + ")";
            }

        default:
            return escaped;
    }
}

QString RuleEngine::buildNumericQuery(const QString& field, RuleOp op,
                                       const QString& value,
                                       const QStringList& values) const
{
    bool ok;
    double numVal = value.toDouble(&ok);
    if (!ok && value.endsWith("KB")) numVal = value.left(value.length()-2).toDouble(&ok) * 1024;
    if (!ok && value.endsWith("MB")) numVal = value.left(value.length()-2).toDouble(&ok) * 1024*1024;
    if (!ok && value.endsWith("GB")) numVal = value.left(value.length()-2).toDouble(&ok) * 1024*1024*1024;
    if (!ok && value.endsWith("B")) numVal = value.left(value.length()-1).toDouble(&ok);
    if (!ok) return "";

    switch (op) {
        case RuleOp::EQUALS:
            return field + ":" + QString::number(static_cast<int64_t>(numVal));
        case RuleOp::GREATER_THAN:
            return field + ":[" + QString::number(static_cast<int64_t>(numVal)) + "..]";
        case RuleOp::LESS_THAN:
            return field + ":[.." + QString::number(static_cast<int64_t>(numVal)) + "]";
        case RuleOp::BETWEEN:
            if (values.size() >= 2) {
                double v1 = values[0].toDouble(&ok);
                if (!ok) return "";
                double v2 = values[1].toDouble(&ok);
                if (!ok) return "";
                return field + ":[" + QString::number(static_cast<int64_t>(v1))
                       + ".." + QString::number(static_cast<int64_t>(v2)) + "]";
            }
            return "";
        default:
            return "";
    }
}

QString RuleEngine::buildDateQuery(const QString& field, RuleOp op,
                                    const QString& value,
                                    const QStringList& values) const
{
    // Convert date string to YYYYMMDD format
    auto toDateStr = [](const QString& s) -> QString {
        QDateTime dt = QDateTime::fromString(s, "yyyy-MM-dd");
        if (!dt.isValid()) dt = QDateTime::fromString(s, "yyyy/MM/dd");
        if (!dt.isValid()) dt = QDateTime::fromString(s, "yyyyMMdd");
        if (dt.isValid()) return dt.toString("yyyyMMdd");
        return s;  // Return as-is if unparseable
    };

    QString dateStr = toDateStr(value);
    if (dateStr.isEmpty()) return "";

    switch (op) {
        case RuleOp::EQUALS:
            return field + ":" + dateStr;
        case RuleOp::GREATER_THAN:
            return field + ":[" + dateStr + "..]";
        case RuleOp::LESS_THAN:
            return field + ":[.." + dateStr + "]";
        case RuleOp::BETWEEN:
            if (values.size() >= 2) {
                return field + ":[" + toDateStr(values[0]) + ".." + toDateStr(values[1]) + "]";
            }
            return "";
        default:
            return dateStr;
    }
}

QString RuleEngine::parseAdvancedQuery(const QString& queryStr) const
{
    if (queryStr.trimmed().isEmpty()) return "";

    QString processed = queryStr;

    // Process field:value (already supported by Xapian QueryParser)
    // We just need to normalize the query

    // Process range syntax: field:>value -> field:[value..]
    processed.replace(QRegularExpression(R"((\w+):>([^\s)]+))"), R"(\1:[\2..])");
    processed.replace(QRegularExpression(R"((\w+):<([^\s)]+))"), R"(\1:[..\2])");

    // Process field:value1..value2 -> field:[value1..value2]
    processed.replace(QRegularExpression(R"((\w+):([^\s)]+)\.\.([^\s)]+))"),
                      R"(\1:[\2..\3])");

    // Ensure boolean operators are uppercase
    {
        QRegularExpression re(R"(\b(and|or|not)\b)",
                               QRegularExpression::CaseInsensitiveOption);
        QString result;
        int lastEnd = 0;
        auto it = re.globalMatch(processed);
        while (it.hasNext()) {
            auto match = it.next();
            result += processed.mid(lastEnd, match.capturedStart() - lastEnd);
            result += match.captured().toUpper();
            lastEnd = match.capturedEnd();
        }
        result += processed.mid(lastEnd);
        processed = result;
    }

    return processed;
}

SearchRule RuleEngine::queryToRule(const QString& queryStr,
                                    const QString& name,
                                    const QString& desc) const
{
    SearchRule rule(name.isEmpty() ? "自定义规则" : name, desc);
    if (!queryStr.trimmed().isEmpty()) {
        rule.addCondition(RuleField::CONTENT, RuleOp::CONTAINS, queryStr.trimmed());
    }
    return rule;
}
