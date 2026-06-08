#ifndef GRAPH_H
#define GRAPH_H

#include "GraphTypes.h"

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

/// 图存储 — 邻接表实现，同时支持有向边和无向边
class Graph {
public:
    Graph() = default;

    // ── 顶点操作 ──

    /// 添加顶点；已存在则忽略。返回顶点引用
    const Vertex& addVertex(const std::string& name);

    /// 检查顶点是否存在
    bool hasVertex(const std::string& name) const;

    /// 获取所有顶点名
    std::vector<std::string> getAllVertexNames() const;

    /// 获取顶点数量
    size_t vertexCount() const;

    /// 获取顶点引用
    const Vertex& getVertex(const std::string& name) const;

    // ── 边操作 ──

    /// 添加边；重复边（同方向/同端点）忽略。返回是否成功添加
    bool addEdge(const std::string& from, const std::string& to,
                 double weight = 1.0, bool directed = false);

    /// 获取从某顶点出发的所有邻边（无向边也包含在邻接表中）
    const std::vector<Edge>& getAdjacent(const std::string& name) const;

    /// 获取所有边（去重）
    std::vector<Edge> getAllEdges() const;

    /// 获取边数（去重后）
    size_t edgeCount() const;

    /// 获取某顶点的度数（无向图：邻边数；有向图：仅计算出度）
    size_t degree(const std::string& name) const;

    /// 获取某顶点的入度（有向图有效，无向图返回0）
    size_t inDegree(const std::string& name) const;

    /// 是否存在有向边
    bool hasDirectedEdges() const;

    // ── 管理 ──

    /// 清空所有数据
    void clear();

    /// 获取下一个自动 ID
    int nextId() const { return next_id_; }

private:
    std::unordered_map<std::string, Vertex> vertices_;
    std::unordered_map<std::string, std::vector<Edge>> adj_;
    std::unordered_set<std::string> edge_keys_;
    int next_id_ = 1;
    bool has_directed_ = false;
};

#endif // GRAPH_H
