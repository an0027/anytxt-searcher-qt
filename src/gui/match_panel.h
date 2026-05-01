/*
 * match_panel.h - 命中段落面板（右下）

功能说明：搜索后显示包含关键词的段落摘录，
点击段落跳转到文件内容中的对应位置。
 */

#ifndef ANYTXT_MATCH_PANEL_H
#define ANYTXT_MATCH_PANEL_H

#include <QWidget>
#include <QListWidget>
#include <QLabel>
#include <QVBoxLayout>
#include "core/document.h"

struct MatchItem {
    int paragraphIndex;     // line/paragraph number in content
    QString snippet;        // truncated text around the match
    int matchPos;           // position in content
};

class MatchPanel : public QWidget {
    Q_OBJECT
public:
    explicit MatchPanel(QWidget* parent = nullptr);
    ~MatchPanel() override = default;

    void setMatches(const QVector<MatchItem>& items, int totalHits);
    void clear();

signals:
    void paragraphClicked(int paragraphIndex);

private:
    void setupUI();
    QListWidget* m_list;
    QLabel* m_label;
};

#endif // ANYTXT_MATCH_PANEL_H
