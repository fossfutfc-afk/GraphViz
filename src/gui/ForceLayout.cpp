#include "ForceLayout.h"
#include "Graph.h"

#include <algorithm>
#include <cmath>
#include <QRandomGenerator>
#include <QVector>

QHash<QString, QPointF> ForceLayout::compute(const Graph& graph,
                                              int width, int height)
{
    QHash<QString, QPointF> positions;

    auto vertexIds = graph.getAllVertexNames();
    if (vertexIds.empty()) return positions;

    const int n = static_cast<int>(vertexIds.size());
    const double area = static_cast<double>(width) * height;
    const double k = std::sqrt(area / n);           // 理想节点间距
    const int iterations = 70;
    const double margin = 50.0;

    // 1. 随机初始化坐标
    QRandomGenerator rng(42);  // 固定种子，每次布局一致
    QVector<QString> ids;
    ids.reserve(n);
    for (const auto& id : vertexIds) {
        auto qid = QString::fromStdString(id);
        ids.append(qid);
        positions[qid] = QPointF(
            margin + rng.bounded(width - 2.0 * margin),
            margin + rng.bounded(height - 2.0 * margin));
    }

    // 2. 构建邻接表（无向边展开为双向）
    QHash<QString, QVector<QString>> adj;
    for (const auto& name : vertexIds) {
        auto qname = QString::fromStdString(name);
        for (const auto& e : graph.getAdjacent(name)) {
            auto to = QString::fromStdString(e.to);
            adj[qname].append(to);
        }
    }

    // 3. 迭代
    double t = width / 10.0;                     // 初始温度
    const double cooling = t / iterations;       // 每步降温

    for (int iter = 0; iter < iterations; ++iter) {
        // 位移累加器
        QHash<QString, QPointF> disp;

        // 3a. 排斥力 — 所有节点对之间
        for (int i = 0; i < n; ++i) {
            for (int j = i + 1; j < n; ++j) {
                QPointF delta = positions[ids[i]] - positions[ids[j]];
                double dist = std::hypot(delta.x(), delta.y());
                if (dist < 1.0) dist = 1.0;

                double force = k * k / dist;
                QPointF d = delta / dist * force;
                disp[ids[i]] += d;
                disp[ids[j]] -= d;
            }
        }

        // 3b. 吸引力 — 所有边端点之间
        for (const auto& name : vertexIds) {
            auto qname = QString::fromStdString(name);
            for (const auto& e : graph.getAdjacent(name)) {
                auto to = QString::fromStdString(e.to);

                // 避免重复计算无向边（from <= to 时处理）
                std::string f = e.from, t = e.to;
                if (!e.directed && f > t) continue;

                QPointF delta = positions[qname] - positions[to];
                double dist = std::hypot(delta.x(), delta.y());
                if (dist < 1.0) dist = 1.0;

                double force = dist * dist / k;
                QPointF d = delta / dist * force;
                disp[qname] -= d;
                disp[to] += d;
            }
        }

        // 3c. 应用位移（受温度限制）
        for (const auto& id : ids) {
            QPointF d = disp[id];
            double dist = std::hypot(d.x(), d.y());
            if (dist < 1.0) continue;

            QPointF limited = d / dist * std::min(dist, t);
            double nx = std::clamp(positions[id].x() + limited.x(),
                                   margin, double(width) - margin);
            double ny = std::clamp(positions[id].y() + limited.y(),
                                   margin, double(height) - margin);
            positions[id] = QPointF(nx, ny);
        }

        t -= cooling;
    }

    return positions;
}
