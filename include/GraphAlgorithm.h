#ifndef GRAPHALGORITHM_H
#define GRAPHALGORITHM_H

#include "Graph.h"

#include <string>
#include <utility>
#include <vector>

// ── 通用结果结构体 ──

/// 最短路径结果
struct PathResult {
    bool found = false;
    double totalWeight = 0.0;
    std::vector<std::string> nodes;     // 路径顶点序列
    std::vector<std::string> edges;     // 路径边序列（格式 "from|to"）
};

/// 连通分量结果
struct ComponentResult {
    std::vector<std::vector<std::string>> components;
};

/// MST 结果
struct MSTResult {
    bool connected = true;
    double totalWeight = 0.0;
    std::vector<std::string> edges;     // "from|to"
};

/// 关节点 & 桥结果
struct CriticalResult {
    std::vector<std::string> articulationPoints;
    std::vector<std::pair<std::string, std::string>> bridges;
};

/// 欧拉回路/通路结果
struct EulerResult {
    bool exists = false;
    bool isCircuit = false;           // true=回路, false=通路
    std::vector<std::string> nodes;   // 顶点序列 [a, b, c, ..., a]  回路首尾相同
};

/// 哈密顿回路/通路结果
struct HamiltonResult {
    bool found = false;
    bool isCircuit = false;
    std::vector<std::string> nodes;
    std::string message;              // 额外说明（如"图过大，无法计算"）
};

// ── 算法类 ──

class GraphAlgorithm {
public:
    // ── 连通分量（BFS，无向图） ──
    static ComponentResult connectedComponents(const Graph& graph);

    // ── 强连通分量（Kosaraju，有向图） ──
    static ComponentResult stronglyConnectedComponents(const Graph& graph);

    // ── 最短路径（Dijkstra） ──
    static PathResult shortestPath(const Graph& graph,
                                   const std::string& from,
                                   const std::string& to);

    // ── 最小生成树（Kruskal，无向图） ──
    static MSTResult minimumSpanningTree(const Graph& graph);

    // ── 关节点 & 桥（Tarjan） ──
    static CriticalResult findCritical(const Graph& graph);

    // ── 欧拉回路（Hierholzer，无向图） ──
    static EulerResult eulerCircuit(const Graph& graph);

    // ── 欧拉通路（Hierholzer 变体，无向图） ──
    static EulerResult eulerTrail(const Graph& graph);

    // ── 哈密顿回路（回溯 + 剪枝） ──
    static HamiltonResult hamiltonCircuit(const Graph& graph);

    // ── 哈密顿通路（回溯 + 剪枝） ──
    static HamiltonResult hamiltonPath(const Graph& graph);
};

#endif // GRAPHALGORITHM_H
