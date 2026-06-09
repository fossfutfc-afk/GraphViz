#ifndef GRAPHTEXTEDIT_H
#define GRAPHTEXTEDIT_H

#include <QPlainTextEdit>

/// QPlainTextEdit 子类 — 添加 Shift+Tab 前向缩进支持
class GraphTextEdit : public QPlainTextEdit {
    Q_OBJECT

public:
    explicit GraphTextEdit(QWidget *parent = nullptr)
        : QPlainTextEdit(parent) {}

protected:
    void keyPressEvent(QKeyEvent *event) override;
};

#endif // GRAPHTEXTEDIT_H
