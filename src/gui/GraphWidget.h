#ifndef GRAPHWIDGET_H
#define GRAPHWIDGET_H

#include <QHash>
#include <QPair>
#include <QPointF>
#include <QSet>
#include <QString>
#include <QVector>
#include <QWidget>

class Graph;

/// 高亮类型枚举
enum class HighlightType {
    None,
    Path,            // 最短路径 — 绿色
    Critical,        // 关节点/桥 — 红色节点/紫色边
    MST,             // 最小生成树 — 橙色边
    Euler,           // 欧拉回路/通路 — 蓝色
    Hamilton,        // 哈密顿回路/通路 — 青色
    Component,       // 连通分量 — 多色
};

/// 图可视化组件 — QPainter 绘制节点、边、高亮
class GraphWidget : public QWidget {
    Q_OBJECT

public:
    explicit GraphWidget(QWidget *parent = nullptr);
    ~GraphWidget() override = default;

    /// 设置图数据，重新计算布局
    void setGraph(Graph* graph);

    /// 获取当前坐标引用（用于外部修改，如拖动后同步）
    QHash<QString, QPointF>& positions() { return m_positions; }

    /// 设置路径高亮（顶点序列）
    void setPathHighlight(const QVector<QString>& nodes);

    /// 设置关键节点和关键边
    void setCriticalHighlight(const QVector<QString>& nodes,
                              const QVector<QPair<QString, QString>>& edges);

    /// 设置边高亮（MST、欧拉、哈密顿）
    void setEdgeHighlight(const QVector<QString>& edges,
                          HighlightType type);

    /// 设置连通分量高亮
    void setComponentHighlight(const QVector<QVector<QString>>& components);

    /// 清除所有高亮
    void clearHighlights();

    /// 计算布局
    void computeLayout();

    /// 手动设置位置（用于外部预先计算）
    void setPositions(const QHash<QString, QPointF>& pos);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    /// 获取某坐标处的顶点名（命中检测），未命中返回空字符串
    QString hitTest(const QPointF& screenPos) const;

    Graph* m_graph = nullptr;

    // ── 布局 ──
    QHash<QString, QPointF> m_positions;

    // ── 高亮数据 ──
    HighlightType m_highlightType = HighlightType::None;
    QVector<QString> m_pathNodes;
    QSet<QPair<QString, QString>> m_highlightEdges;  // 标准化键
    QSet<QString> m_highlightNodes;
    QVector<QVector<QString>> m_components;
    QHash<QString, QColor> m_componentColors;

    // ── 拖动状态 ──
    bool m_dragging = false;
    QString m_dragNode;       // 正在拖动的顶点名
    QPointF m_dragOffset;     // 鼠标相对于顶点中心的偏移（屏幕坐标）
};

#endif // GRAPHWIDGET_H
