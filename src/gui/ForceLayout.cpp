#include "ForceLayout.h"
#include "Graph.h"

#define _USE_MATH_DEFINES
#include <cmath>
#include <algorithm>
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
    const int iterations = 100;
    const double margin = 50.0;

    // 1. 环形初始布局（顶点均匀分布，防止初始聚集导致边重合）
    QRandomGenerator rng(42);
    QVector<QString> ids;
    ids.reserve(n);
    const double centerX = width / 2.0;
    const double centerY = height / 2.0;
    const double radius = std::min(width, height) * 0.35;  // 环形半径
    for (int i = 0; i < n; ++i) {
        double angle = 2.0 * M_PI * i / n;
        // 加小幅随机抖动，避免对称网格导致力平衡停滞
        double jitterX = (rng.bounded(1.0) - 0.5) * 20.0;
        double jitterY = (rng.bounded(1.0) - 0.5) * 20.0;
        auto qid = QString::fromStdString(vertexIds[i]);
        ids.append(qid);
        positions[qid] = QPointF(
            centerX + radius * std::cos(angle) + jitterX,
            centerY + radius * std::sin(angle) + jitterY);
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
