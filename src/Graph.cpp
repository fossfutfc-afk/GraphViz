#include "Graph.h"

#include <algorithm>

// ── Edge::makeKey ──

std::string Edge::makeKey() const {
    if (directed) {
        return from + "|" + to;
    } else {
        // 无向：字典序小的在前
        return (from <= to) ? (from + "|" + to) : (to + "|" + from);
    }
}

// ── Graph 实现 ──

const Vertex& Graph::addVertex(const std::string& name) {
    auto it = vertices_.find(name);
    if (it != vertices_.end())
        return it->second;

    Vertex v;
    v.name = name;
    v.id = next_id_++;
    auto [ins, _] = vertices_.emplace(name, v);
    // 确保邻接表有入口
    if (adj_.find(name) == adj_.end())
        adj_[name] = {};
    return ins->second;
}

bool Graph::hasVertex(const std::string& name) const {
    return vertices_.find(name) != vertices_.end();
}

std::vector<std::string> Graph::getAllVertexNames() const {
    std::vector<std::string> names;
    names.reserve(vertices_.size());
    for (const auto& kv : vertices_)
        names.push_back(kv.first);
    return names;
}

size_t Graph::vertexCount() const {
    return vertices_.size();
}

const Vertex& Graph::getVertex(const std::string& name) const {
    static const Vertex dummy;
    auto it = vertices_.find(name);
    if (it != vertices_.end())
        return it->second;
    return dummy;
}

bool Graph::addEdge(const std::string& from, const std::string& to,
                     double weight, bool directed)
{
    // 自动创建不存在的顶点
    addVertex(from);
    addVertex(to);

    Edge e;
    e.from = from;
    e.to = to;
    e.weight = weight;
    e.directed = directed;

    std::string key = e.makeKey();
    if (edge_keys_.find(key) != edge_keys_.end())
        return false;  // 重复边

    edge_keys_.insert(key);

    if (directed) {
        has_directed_ = true;
        adj_[from].push_back(e);
    } else {
        // 无向边：两端都添加
        adj_[from].push_back(e);
        Edge rev = e;
        rev.from = to;
        rev.to = from;
        adj_[to].push_back(rev);
    }

    return true;
}

const std::vector<Edge>& Graph::getAdjacent(const std::string& name) const {
    static const std::vector<Edge> empty;
    auto it = adj_.find(name);
    if (it != adj_.end())
        return it->second;
    return empty;
}

std::vector<Edge> Graph::getAllEdges() const {
    std::vector<Edge> edges;
    edges.reserve(edge_keys_.size());

    // 遍历邻接表，用 edge_keys 去重
    std::unordered_set<std::string> seen;
    for (const auto& kv : adj_) {
        for (const auto& e : kv.second) {
            std::string key = e.makeKey();
            if (seen.find(key) == seen.end()) {
                seen.insert(key);
                edges.push_back(e);
            }
        }
    }
    return edges;
}

size_t Graph::edgeCount() const {
    return edge_keys_.size();
}

size_t Graph::degree(const std::string& name) const {
    auto it = adj_.find(name);
    if (it == adj_.end()) return 0;
    return it->second.size();
}

size_t Graph::inDegree(const std::string& name) const {
    if (!has_directed_) return 0;
    size_t count = 0;
    for (const auto& kv : adj_) {
        if (kv.first == name) continue;
        for (const auto& e : kv.second) {
            if (e.directed && e.to == name)
                ++count;
        }
    }
    return count;
}

bool Graph::hasDirectedEdges() const {
    return has_directed_;
}

void Graph::clear() {
    vertices_.clear();
    adj_.clear();
    edge_keys_.clear();
    next_id_ = 1;
    has_directed_ = false;
}
