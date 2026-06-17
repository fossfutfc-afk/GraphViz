#include "ForceLayout.h"
#include "Graph.h"
#include "GraphAlgorithm.h"

#define _USE_MATH_DEFINES
#include <cmath>
#include <algorithm>
#include <QPair>
#include <QRandomGenerator>
#include <QSet>
#include <QVector>

// ── 分量级 FR 主循环 ──
// 在给定区域内对指定顶点集合运行 Fruchterman-Reingold 布局。
// 不关心其他分量——排斥力仅在分量内部计算。
static QHash<QString, QPointF> runForceLayout(
    const std::vector<std::string>& vertexIds,
    const Graph& graph,
    double areaW, double areaH,
    int seed)
{
    QHash<QString, QPointF> positions;

    const int n = static_cast<int>(vertexIds.size());
    if (n == 0) return positions;

    const double area = areaW * areaH;
    // 小图增大理想间距，防止挤在一起
    const double spacingBoost = (n <= 4) ? 1.2 : 1.0;
    const double k = std::sqrt(area / n) * spacingBoost;
    const int iterations = 200;
    const double margin = std::min(areaW, areaH) * 0.1;

    // 1. 环形初始布局（顶点均匀分布，防止初始聚集导致边重合）
    QRandomGenerator rng(seed);
    QVector<QString> ids;
    ids.reserve(n);
    const double centerX = areaW / 2.0;
    const double centerY = areaH / 2.0;
    const double radius = std::min(areaW, areaH) * 0.35;
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

    // 2. 迭代（乘法降温，更平滑）
    double t = std::min(areaW, areaH) * 0.5;
    const double cooling = std::pow(0.01, 1.0 / iterations);

    for (int iter = 0; iter < iterations; ++iter) {
        QHash<QString, QPointF> disp;

        // 2a. 排斥力（标准 FR 公式，仅在分量内部）
        for (int i = 0; i < n; ++i) {
            for (int j = i + 1; j < n; ++j) {
                QPointF delta = positions[ids[i]] - positions[ids[j]];
                double dist = std::hypot(delta.x(), delta.y());
                if (dist < 1.0) dist = 1.0;

                double force = 1.0 * k * k / dist;
                QPointF d = delta / dist * force;
                disp[ids[i]] += d;
                disp[ids[j]] -= d;
            }
        }

        // 2b. 吸引力 — 每对端点仅计一次（平行边去重）
        QSet<QPair<QString, QString>> attracted;
        for (const auto& name : vertexIds) {
            auto qname = QString::fromStdString(name);
            for (const auto& e : graph.getAdjacent(name)) {
                auto to = QString::fromStdString(e.to);
                if (qname == to) continue;  // skip self-loops

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

        // 2c. 应用位移（受温度限制，软边界：到边缘的距离减弱）
        for (const auto& id : ids) {
            QPointF d = disp[id];
            double dlen = std::hypot(d.x(), d.y());
            if (dlen < 0.01) continue;

            QPointF limited = d / dlen * std::min(dlen, t);
            double nx = positions[id].x() + limited.x();
            double ny = positions[id].y() + limited.y();

            // 软边界：越界时温和拉回
            if (nx < margin)        nx = nx + (margin - nx) * 0.8;
            if (nx > areaW - margin)
                nx = nx - (nx - areaW + margin) * 0.8;
            if (ny < margin)        ny = ny + (margin - ny) * 0.8;
            if (ny > areaH - margin)
                ny = ny - (ny - areaH + margin) * 0.8;

            positions[id] = QPointF(nx, ny);
        }

        t *= cooling;
    }

    return positions;
}

// ── 顶层入口 ──
QHash<QString, QPointF> ForceLayout::compute(const Graph& graph,
                                              int width, int height)
{
    auto vertexIds = graph.getAllVertexNames();
    if (vertexIds.empty()) return {};

    // 检测连通分量
    auto compResult = GraphAlgorithm::connectedComponents(graph);
    const auto& comps = compResult.components;

    // 单分量 → 走原逻辑（完全不变）
    if (comps.size() <= 1) {
        return runForceLayout(vertexIds, graph,
                              static_cast<double>(width),
                              static_cast<double>(height), 42);
    }

    // 多分量 → 每个分量独立 FR + 网格排列
    const int nComps = static_cast<int>(comps.size());
    const int cols = static_cast<int>(std::ceil(std::sqrt(nComps)));
    const int rows = static_cast<int>(std::ceil(static_cast<double>(nComps) / cols));
    const double cellW = static_cast<double>(width) / cols;
    const double cellH = static_cast<double>(height) / rows;
    const double gap = std::min(cellW, cellH) * 0.15;
    const double effW = cellW - gap;
    const double effH = cellH - gap;

    QHash<QString, QPointF> allPositions;

    for (int ci = 0; ci < nComps; ++ci) {
        const auto& comp = comps[ci];
        const int compN = static_cast<int>(comp.size());

        // 网格单元中心
        const int col = ci % cols;
        const int row = ci / cols;
        const double cellCenterX = (col + 0.5) * cellW;
        const double cellCenterY = (row + 0.5) * cellH;

        QHash<QString, QPointF> compPos;

        if (compN == 1) {
            // 单顶点 → 直接放单元中心，不跑 FR
            compPos[QString::fromStdString(comp[0])] = QPointF(cellCenterX, cellCenterY);
        } else {
            // 多顶点 → 在有效区域内独立 FR
            compPos = runForceLayout(comp, graph, effW, effH, 42 + ci);

            // 计算质心
            double cx = 0.0, cy = 0.0;
            for (const auto& pos : compPos) {
                cx += pos.x();
                cy += pos.y();
            }
            cx /= compN;
            cy /= compN;

            // 平移到网格单元中心
            const double dx = cellCenterX - cx;
            const double dy = cellCenterY - cy;
            for (auto& pos : compPos) {
                pos.rx() += dx;
                pos.ry() += dy;
            }
        }

        // 合并到总结果
        for (auto it = compPos.begin(); it != compPos.end(); ++it)
            allPositions[it.key()] = it.value();
    }

    return allPositions;
}
