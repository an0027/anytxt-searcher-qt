/*
 * rule_engine.h - 搜索规则引擎

功能说明：支持多种条件组合的高级搜索规则引擎。
提供字段搜索（标题/文件名/类型/大小/日期等）、
布尔操作符（AND/OR/NOT）、范围查询、通配符搜索、
规则序列化保存/加载等功能。
 */

#ifndef ANYTXT_RULE_ENGINE_H
#define ANYTXT_RULE_ENGINE_H

#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <QJsonObject>
#include <QJsonArray>
#include <QVector>
#include <QPair>

// === Rule Operators ===
enum class RuleOp {
    AND,            // Combine conditions with AND
    OR,             // Combine conditions with OR
    NOT,            // Negate a condition
    CONTAINS,       // Text contains value
    EQUALS,         // Exact match
    STARTS_WITH,    // Text starts with
    ENDS_WITH,      // Text ends with
    GREATER_THAN,   // Numeric/date >
    LESS_THAN,      // Numeric/date <
    BETWEEN,        // Numeric/date between
    IN,             // Value in list
    REGEX           // Regex match
};

// === Rule Fields ===
enum class RuleField {
    CONTENT,
    TITLE,
    FILENAME,
    FILE_PATH,
    FILE_TYPE,
    FILE_EXTENSION,
    FILE_SIZE,
    MODIFIED_DATE,
    CREATED_DATE,
    AUTHOR,
    KEYWORDS,
    MIME_TYPE
};

// === Single Condition ===
struct RuleCondition {
    RuleField field = RuleField::CONTENT;
    RuleOp op = RuleOp::CONTAINS;
    QString value;
    QStringList valueList;   // For IN, BETWEEN operators
    bool negate = false;

    QJsonObject toJson() const;
    static RuleCondition fromJson(const QJsonObject& obj);
};

// === Search Rule (group of conditions) ===
class SearchRule {
public:
    SearchRule() = default;
    SearchRule(const QString& name, const QString& desc = QString(),
               RuleOp op = RuleOp::AND);

    SearchRule& addCondition(const RuleCondition& condition);
    SearchRule& addCondition(RuleField field, RuleOp op,
                             const QString& value, bool negate = false);
    SearchRule& addCondition(RuleField field, RuleOp op,
                             const QStringList& values, bool negate = false);

    QString name() const { return m_name; }
    void setName(const QString& n) { m_name = n; }

    QString description() const { return m_description; }
    void setDescription(const QString& d) { m_description = d; }

    RuleOp combineOp() const { return m_combineOp; }
    void setCombineOp(RuleOp op) { m_combineOp = op; }

    const QVector<RuleCondition>& conditions() const { return m_conditions; }
    int conditionCount() const { return m_conditions.size(); }
    bool isValid() const { return !m_conditions.isEmpty(); }

    // Serialization
    QJsonObject toJson() const;
    static SearchRule fromJson(const QJsonObject& obj);
    QString toJsonString() const;
    static SearchRule fromJsonString(const QString& json);

private:
    QString m_name;
    QString m_description;
    RuleOp m_combineOp = RuleOp::AND;
    QVector<RuleCondition> m_conditions;
};

// === Rule Engine ===
class RuleEngine {
public:
    RuleEngine();

    // Convert SearchRule to Xapian query string
    QString parseRule(const SearchRule& rule) const;

    // Parse advanced query syntax
    // Supports: AND OR NOT, field:value, size:>1MB, date:>2024-01-01
    //           "phrase", wildcard*, (grouping)
    QString parseAdvancedQuery(const QString& queryStr) const;

    // Convert advanced query string to SearchRule
    SearchRule queryToRule(const QString& queryStr,
                           const QString& name = QString(),
                           const QString& desc = QString()) const;

    // Get display name for field
    static QString fieldDisplayName(RuleField field);
    static QString fieldKey(RuleField field);
    static QString fieldPrefix(RuleField field);
    static bool isNumericField(RuleField field);
    static bool isDateField(RuleField field);
    static QString opDisplayName(RuleOp op);

    // Supported fields list
    static QVector<QPair<RuleField, QString>> supportedFields();

    // Conversion helpers
    static QString prettyValue(const QString& field, const QString& value);
    static QString formatFileSize(int64_t bytes);

private:
    QString buildConditionQuery(const RuleCondition& cond) const;
    QString buildTextQuery(const QString& field, RuleOp op,
                           const QString& value) const;
    QString buildNumericQuery(const QString& field, RuleOp op,
                              const QString& value,
                              const QStringList& values) const;
    QString buildDateQuery(const QString& field, RuleOp op,
                           const QString& value,
                           const QStringList& values) const;
    QString escapeValue(const QString& value) const;
    QString fieldToPrefix(const QString& field) const;

    mutable QMap<QString, QString> m_prefixCache;
};

#endif // ANYTXT_RULE_ENGINE_H
