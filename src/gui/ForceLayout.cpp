#include "ForceLayout.h"
#include "Graph.h"

#define _USE_MATH_DEFINES
#include <cmath>
#include <algorithm>
#include <QPair>
#include <QRandomGenerator>
#include <QSet>
#include <QVector>

QHash<QString, QPointF> ForceLayout::compute(const Graph& graph,
                                              int width, int height)
{
    QHash<QString, QPointF> positions;

    auto vertexIds = graph.getAllVertexNames();
    if (vertexIds.empty()) return positions;

    const int n = static_cast<int>(vertexIds.size());
    const double area = static_cast<double>(width) * height;
    // 小图增大理想间距，防止挤在一起
    const double spacingBoost = (n <= 4) ? 1.8 : 1.0;
    const double k = std::sqrt(area / n) * spacingBoost;
    const int iterations = 150;
    const double margin = 70.0;

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

    // 3. 迭代（乘法降温，更平滑）
    double t = std::min(width, height) * 0.5;    // 初始温度
    const double cooling = std::pow(0.01, 1.0 / iterations);  // 末温 ≈ 1% 初温

    for (int iter = 0; iter < iterations; ++iter) {
        QHash<QString, QPointF> disp;

        // 3a. 排斥力（增强 1.5x，防止顶点挤压到边缘共线）
        for (int i = 0; i < n; ++i) {
            for (int j = i + 1; j < n; ++j) {
                QPointF delta = positions[ids[i]] - positions[ids[j]];
                double dist = std::hypot(delta.x(), delta.y());
                if (dist < 1.0) dist = 1.0;

                double force = 1.5 * k * k / dist;
                QPointF d = delta / dist * force;
                disp[ids[i]] += d;
                disp[ids[j]] -= d;
            }
        }

        // 3b. 吸引力 — 每对端点仅计一次
        QSet<QPair<QString, QString>> attracted;
        for (const auto& name : vertexIds) {
            auto qname = QString::fromStdString(name);
            for (const auto& e : graph.getAdjacent(name)) {
                auto to = QString::fromStdString(e.to);
                if (qname == to) continue;

                auto pair = (qname <= to) ? qMakePair(qname, to)
                                          : qMakePair(to, qname);
                if (attracted.contains(pair)) continue;
                attracted.insert(pair);

                QPointF delta = positions[qname] - positions[to];
                double dist = std::hypot(delta.x(), delta.y());
                if (dist < 1.0) dist = 1.0;

                double force = dist * dist / k;
                QPointF d = delta / dist * force;
                disp[qname] -= d;
                disp[to] += d;
            }
        }

        // 3c. 应用位移（受温度限制，软边界：到边缘的距离减弱）
        for (const auto& id : ids) {
            QPointF d = disp[id];
            double dlen = std::hypot(d.x(), d.y());
            if (dlen < 0.01) continue;

            QPointF limited = d / dlen * std::min(dlen, t);
            double nx = positions[id].x() + limited.x();
            double ny = positions[id].y() + limited.y();

            // 软边界：越界时温和拉回
            if (nx < margin)        nx = nx + (margin - nx) * 0.8;
            if (nx > double(width)  - margin)
                nx = nx - (nx - double(width) + margin) * 0.8;
            if (ny < margin)        ny = ny + (margin - ny) * 0.8;
            if (ny > double(height) - margin)
                ny = ny - (ny - double(height) + margin) * 0.8;

            positions[id] = QPointF(nx, ny);
        }

        t *= cooling;
    }

    return positions;
}
