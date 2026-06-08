#include "GraphWidget.h"
#include "ForceLayout.h"
#include "Graph.h"

#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QPen>
#include <cmath>

// ── 绘制常量 ──
static constexpr double kNODE_RADIUS    = 20.0;
static constexpr double kARROW_SIZE     = 10.0;
static constexpr double kPADDING        = 50.0;
static constexpr int    kFONT_SIZE      = 9;
static constexpr int    kWEIGHT_FONT_SIZE = 8;
static const QColor     kNODE_FILL("#E0E0E0");
static const QColor     kNODE_BORDER(Qt::black);

// ── 高亮颜色 ──
static const QColor kPathColor(0, 180, 0);         // 绿色
static const QColor kCriticalNodeColor(Qt::red);
static const QColor kCriticalEdgeColor(128, 0, 128); // 紫色
static const QColor kMSTColor(255, 140, 0);         // 橙色
static const QColor kEulerColor(0, 100, 200);       // 蓝色
static const QColor kHamiltonColor(0, 180, 180);    // 青色

// 连通分量颜色表
static const QColor kComponentColors[] = {
    QColor(230, 25, 75),   QColor(60, 180, 75),   QColor(255, 225, 25),
    QColor(0, 130, 200),   QColor(245, 130, 48),  QColor(145, 30, 180),
    QColor(70, 240, 240),  QColor(240, 50, 230),  QColor(210, 245, 60),
    QColor(250, 190, 212), QColor(0, 128, 128),   QColor(220, 190, 255),
};

/// 根据位置数据计算视图变换，处理单顶点/零范围退化情况
static ViewTransform computeViewTransform(const QHash<QString, QPointF>& positions,
                                           double widgetW, double widgetH)
{
    double minX = 1e9, minY = 1e9, maxX = -1e9, maxY = -1e9;
    for (auto it = positions.begin(); it != positions.end(); ++it) {
        minX = std::min(minX, it->x());
        minY = std::min(minY, it->y());
        maxX = std::max(maxX, it->x());
        maxY = std::max(maxY, it->y());
    }
    double dataW = maxX - minX;
    double dataH = maxY - minY;

    ViewTransform tr;
    const double availW = widgetW  - 2.0 * kPADDING;
    const double availH = widgetH - 2.0 * kPADDING;

    if (dataW < 1.0 && dataH < 1.0) {
        // 退化情况：单顶点或所有顶点同位置 — 居中显示
        tr.scaleX = 1.0;
        tr.scaleY = 1.0;
        tr.offsetX = widgetW / 2.0 - minX;
        tr.offsetY = widgetH / 2.0 - minY;
    } else {
        double sw = (dataW < 1.0) ? 1.0 : availW / dataW;
        double sh = (dataH < 1.0) ? 1.0 : availH / dataH;
        double s = std::min(sw, sh);
        tr.scaleX = s;
        tr.scaleY = s;

        // 居中
        tr.offsetX = kPADDING + (availW - dataW * s) / 2.0 - minX * s;
        tr.offsetY = kPADDING + (availH - dataH * s) / 2.0 - minY * s;
    }
    return tr;
}

// ── 辅助：标准化边键 ──
static inline QPair<QString, QString> makeKey(const QString& a, const QString& b) {
    return (a <= b) ? qMakePair(a, b) : qMakePair(b, a);
}

// ── 辅助：绘制箭头 ──
static void drawArrow(QPainter& painter, const QPointF& from,
                       const QPointF& to, double nodeRadius)
{
    // 计算从圆心到圆心的方向
    QPointF dir = to - from;
    double len = std::hypot(dir.x(), dir.y());
    if (len < 1.0) return;
    QPointF unit = dir / len;

    // 终点圆边界上的点
    QPointF endPt = to - unit * nodeRadius;

    // 箭头三角形
    QPointF perp(-unit.y(), unit.x());
    QPointF p1 = endPt;
    QPointF p2 = endPt - unit * kARROW_SIZE + perp * (kARROW_SIZE * 0.5);
    QPointF p3 = endPt - unit * kARROW_SIZE - perp * (kARROW_SIZE * 0.5);

    QPainterPath arrowPath;
    arrowPath.moveTo(p1);
    arrowPath.lineTo(p2);
    arrowPath.lineTo(p3);
    arrowPath.closeSubpath();
    painter.drawPath(arrowPath);
}

// ═══════════════════════════════════════════════════════════════
//  GraphWidget 实现
// ═══════════════════════════════════════════════════════════════

GraphWidget::GraphWidget(QWidget *parent)
    : QWidget(parent)
{
    setMouseTracking(true);
    setMinimumSize(400, 300);
}

void GraphWidget::setGraph(Graph* graph)
{
    m_graph = graph;
    clearHighlights();
    computeLayout();
    update();
}

void GraphWidget::setPathHighlight(const QVector<QString>& nodes)
{
    m_highlightType = HighlightType::Path;
    m_pathNodes = nodes;
    m_highlightEdges.clear();
    m_highlightNodes.clear();
    m_components.clear();

    // 构建路径边集合
    for (int i = 0; i + 1 < nodes.size(); ++i)
        m_highlightEdges.insert(makeKey(nodes[i], nodes[i + 1]));
    // 同时添加原始方向键（有向图用）
    for (int i = 0; i + 1 < nodes.size(); ++i)
        m_highlightEdges.insert(qMakePair(nodes[i], nodes[i + 1]));

    update();
}

void GraphWidget::setCriticalHighlight(const QVector<QString>& nodes,
                                        const QVector<QPair<QString, QString>>& edges)
{
    m_highlightType = HighlightType::Critical;
    m_pathNodes.clear();
    m_highlightEdges.clear();
    m_highlightNodes.clear();
    m_components.clear();

    for (const auto& n : nodes)
        m_highlightNodes.insert(n);

    for (const auto& e : edges) {
        m_highlightEdges.insert(makeKey(e.first, e.second));
        m_highlightEdges.insert(qMakePair(e.first, e.second));
    }

    update();
}

void GraphWidget::setEdgeHighlight(const QVector<QString>& edges,
                                    HighlightType type)
{
    m_highlightType = type;
    m_pathNodes.clear();
    m_highlightEdges.clear();
    m_highlightNodes.clear();
    m_components.clear();

    for (const auto& ek : edges) {
        // 期望格式 "from|to"
        int sep = ek.indexOf('|');
        if (sep >= 0) {
            QString a = ek.left(sep);
            QString b = ek.mid(sep + 1);
            m_highlightEdges.insert(makeKey(a, b));
            m_highlightEdges.insert(qMakePair(a, b));
        }
    }

    update();
}

void GraphWidget::setComponentHighlight(const QVector<QVector<QString>>& components)
{
    m_highlightType = HighlightType::Component;
    m_pathNodes.clear();
    m_highlightEdges.clear();
    m_highlightNodes.clear();
    m_components = components;

    // 为每个分量分配颜色
    m_componentColors.clear();
    for (int i = 0; i < components.size(); ++i) {
        QColor color = kComponentColors[i % (sizeof(kComponentColors) / sizeof(kComponentColors[0]))];
        for (const auto& node : components[i])
            m_componentColors[node] = color;
    }

    update();
}

void GraphWidget::clearHighlights()
{
    m_highlightType = HighlightType::None;
    m_pathNodes.clear();
    m_highlightEdges.clear();
    m_highlightNodes.clear();
    m_components.clear();
    m_componentColors.clear();
    update();
}

void GraphWidget::computeLayout()
{
    if (!m_graph) return;
    m_positions = ForceLayout::compute(*m_graph, width(), height());
}

void GraphWidget::setPositions(const QHash<QString, QPointF>& pos)
{
    m_positions = pos;
}

// ── 命中检测 ──
QString GraphWidget::hitTest(const QPointF& screenPos) const
{
    if (!m_graph || m_positions.empty()) return {};

    ViewTransform tr = computeViewTransform(m_positions, width(), height());

    for (auto it = m_positions.begin(); it != m_positions.end(); ++it) {
        QPointF center = tr.map(it->x(), it->y());
        double dx = screenPos.x() - center.x();
        double dy = screenPos.y() - center.y();
        if (std::hypot(dx, dy) <= kNODE_RADIUS + 3.0) {
            return it.key();
        }
    }
    return {};
}

// ── 鼠标事件 ──
void GraphWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        QString node = hitTest(event->position());
        if (!node.isEmpty()) {
            m_dragging = true;
            m_dragNode = node;

            m_dragTransform = computeViewTransform(m_positions, width(), height());
            QPointF dataPos = m_positions[m_dragNode];
            QPointF screenCenter = m_dragTransform.map(dataPos);
            m_dragOffset = event->position() - screenCenter;
            setCursor(Qt::ClosedHandCursor);
        }
    }
    QWidget::mousePressEvent(event);
}

void GraphWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (m_dragging && !m_dragNode.isEmpty()) {
        // 使用拖动开始时的固定变换，防止拖动过程中包围盒变化导致灵敏度漂移
        QPointF newScreenPos = event->position() - m_dragOffset;
        QPointF newDataPos = m_dragTransform.invMap(newScreenPos);
        m_positions[m_dragNode] = newDataPos;
        update();
        return;
    }

    // 非拖动状态：鼠标悬停时改变光标
    if (!m_dragging) {
        QString node = hitTest(event->position());
        if (!node.isEmpty())
            setCursor(Qt::OpenHandCursor);
        else
            setCursor(Qt::ArrowCursor);
    }

    QWidget::mouseMoveEvent(event);
}

void GraphWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && m_dragging) {
        m_dragging = false;
        m_dragNode.clear();
        setCursor(Qt::ArrowCursor);
    }
    QWidget::mouseReleaseEvent(event);
}

// ═══════════════════════════════════════════════════════════════
//  绘制
// ═══════════════════════════════════════════════════════════════

void GraphWidget::paintEvent(QPaintEvent* /*event*/)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.fillRect(rect(), Qt::white);

    if (!m_graph || m_positions.empty()) return;

    // ── 坐标变换 ──
    ViewTransform tr = computeViewTransform(m_positions, width(), height());

    // ── 收集所有边（去重） ──
    std::vector<Edge> allEdges;
    std::unordered_set<std::string> seen;
    for (const auto& name : m_graph->getAllVertexNames()) {
        for (const auto& e : m_graph->getAdjacent(name)) {
            std::string key = e.makeKey();
            if (seen.count(key)) continue;
            seen.insert(key);
            allEdges.push_back(e);
        }
    }

    // ── 辅助：获取边颜色和线宽 ──
    auto edgeStyle = [&](const QString& from, const QString& to)
        -> std::pair<QColor, double>
    {
        auto key = makeKey(from, to);
        auto dirKey = qMakePair(from, to);

        if (m_highlightEdges.contains(key) || m_highlightEdges.contains(dirKey)) {
            switch (m_highlightType) {
                case HighlightType::Path:      return {kPathColor, 4.0};
                case HighlightType::Critical:  return {kCriticalEdgeColor, 3.0};
                case HighlightType::MST:       return {kMSTColor, 3.5};
                case HighlightType::Euler:     return {kEulerColor, 3.5};
                case HighlightType::Hamilton:  return {kHamiltonColor, 3.5};
                default: break;
            }
        }

        // 连通分量：用分量颜色
        if (m_highlightType == HighlightType::Component) {
            QColor compColor = m_componentColors.value(from, Qt::black);
            if (compColor == Qt::black)
                compColor = m_componentColors.value(to, Qt::black);
            return {compColor, 3.0};
        }

        return {Qt::black, 2.0};
    };

    // ── 1. 绘制边 ──
    for (const auto& e : allEdges) {
        auto from = QString::fromStdString(e.from);
        auto to   = QString::fromStdString(e.to);
        auto [color, width] = edgeStyle(from, to);

        QPen pen(color, width);
        painter.setPen(pen);

        QPointF p1 = tr.map(m_positions.value(from));
        QPointF p2 = tr.map(m_positions.value(to));

        // 计算从圆心出发的线段端点
        QPointF dir = p2 - p1;
        double len = std::hypot(dir.x(), dir.y());
        if (len < 1.0) continue;
        QPointF unit = dir / len;

        QPointF startPt = p1 + unit * kNODE_RADIUS;
        QPointF endPt   = p2 - unit * (e.directed ? kNODE_RADIUS + kARROW_SIZE : kNODE_RADIUS);

        painter.drawLine(startPt, endPt);

        // 有向边：画箭头
        if (e.directed) {
            painter.setBrush(color);
            drawArrow(painter, p1, p2, kNODE_RADIUS);
            painter.setBrush(Qt::NoBrush);
        }

        // 权重标签
        if (e.weight != 1.0) {
            QPointF mid = (p1 + p2) / 2.0;
            // 偏移垂直于边的方向
            QPointF perp(-unit.y(), unit.x());
            QPointF labelPos = mid + perp * 12.0;

            QFont wfont = painter.font();
            wfont.setPointSize(kWEIGHT_FONT_SIZE);
            painter.setFont(wfont);
            painter.setPen(QPen(Qt::darkGray));

            QString weightStr;
            if (e.weight == static_cast<int>(e.weight))
                weightStr = QString::number(static_cast<int>(e.weight));
            else
                weightStr = QString::number(e.weight, 'f', 2);

            // 白色背景让文字更可读
            QFontMetrics fm(wfont);
            QRectF textRect = fm.boundingRect(weightStr);
            textRect.moveCenter(labelPos);
            painter.fillRect(textRect.adjusted(-2, -1, 2, 1), QColor(255, 255, 255, 200));
            painter.drawText(textRect, Qt::AlignCenter, weightStr);
        }
    }

    // ── 2. 绘制节点 ──
    QFont font = painter.font();
    font.setPointSize(kFONT_SIZE);
    painter.setFont(font);

    for (auto it = m_positions.begin(); it != m_positions.end(); ++it) {
        QPointF center = tr.map(it->x(), it->y());

        // 填充颜色
        QColor fill = kNODE_FILL;
        if (m_highlightType == HighlightType::Component) {
            fill = m_componentColors.value(it.key(), kNODE_FILL);
        }
        painter.setBrush(fill);

        // 边框
        bool isHighlightNode = m_highlightNodes.contains(it.key());
        if (isHighlightNode)
            painter.setPen(QPen(kCriticalNodeColor, 2.5));
        else
            painter.setPen(QPen(kNODE_BORDER, 1.5));

        painter.drawEllipse(center, kNODE_RADIUS, kNODE_RADIUS);

        // 标签
        painter.setPen(Qt::black);
        painter.setBrush(Qt::NoBrush);
        QRectF textRect(center.x() - kNODE_RADIUS * 2,
                        center.y() - kNODE_RADIUS,
                        kNODE_RADIUS * 4,
                        kNODE_RADIUS * 2);
        painter.drawText(textRect, Qt::AlignCenter, it.key());
    }
}
