#ifndef FORCELAYOUT_H
#define FORCELAYOUT_H

#include <QHash>
#include <QPointF>
#include <QString>

class Graph;

/// Fruchterman-Reingold 力导向布局算法
class ForceLayout {
public:
    /// 计算节点坐标
    /// @param graph  图数据
    /// @param width  布局区域宽度
    /// @param height 布局区域高度
    /// @return 顶点名 → 坐标
    static QHash<QString, QPointF> compute(const Graph& graph,
                                           int width, int height);
};

#endif // FORCELAYOUT_H
