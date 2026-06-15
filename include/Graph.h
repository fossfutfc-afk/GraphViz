#ifndef GRAPH_H
#define GRAPH_H

#include "GraphTypes.h"

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

/// 图存储 — 邻接表实现，同时支持有向边和无向边，支持平行边（重边）
class Graph {
public:
    Graph() = default;

    // ── 顶点操作 ──

    const Vertex& addVertex(const std::string& name);
    bool hasVertex(const std::string& name) const;
    std::vector<std::string> getAllVertexNames() const;
    size_t vertexCount() const;
    const Vertex& getVertex(const std::string& name) const;

    /// 获取顶点显示名（若未设置则返回 internal key 本身）
    std::string getDisplayName(const std::string& internal_key) const;

    /// 通过显示名或内部 key 查找内部 key（用于 from/to 输入框）
    /// 先精确匹配 key，再按 display_name 搜索
    std::string resolveVertexName(const std::string& displayOrKey) const;

    // ── 边操作 ──

    /// 添加边。支持平行边（同端点同方向），每条边分配唯一 id
    /// @param explicit_w 用户是否显式提供了权重
    bool addEdge(const std::string& from, const std::string& to,
                 double weight = 1.0, bool directed = false,
                 bool explicit_w = false);

    const std::vector<Edge>& getAdjacent(const std::string& name) const;

    /// 获取所有边（按 id 去重，保留平行边）
    std::vector<Edge> getAllEdges() const;

    /// 获取边数（含平行边）
    size_t edgeCount() const;

    size_t degree(const std::string& name) const;
    size_t inDegree(const std::string& name) const;
    bool hasDirectedEdges() const;

    /// 是否存在用户显式提供权重的边
    bool hasExplicitWeight() const { return has_explicit_weight_; }

    // ── 管理 ──

    void clear();
    int nextId() const { return next_id_; }

private:
    std::unordered_map<std::string, Vertex> vertices_;
    std::unordered_map<std::string, std::vector<Edge>> adj_;
    std::unordered_set<int> seen_edge_ids_;       // 用于 getAllEdges 去重
    int next_id_ = 1;
    int edge_id_counter_ = 1;
    bool has_directed_ = false;
    bool has_explicit_weight_ = false;
};

#endif // GRAPH_H
