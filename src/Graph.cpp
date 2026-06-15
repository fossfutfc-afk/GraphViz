#include "Graph.h"

#include <algorithm>

// ── Edge::makeKey ──

std::string Edge::makeKey() const {
    if (directed) {
        return from + "|" + to;
    } else {
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

    // 若 name 含 # 后缀，拆分为 display_name + suffix
    size_t hashPos = name.find('#');
    if (hashPos != std::string::npos && hashPos > 0 && hashPos + 1 < name.size()) {
        v.display_name = name.substr(0, hashPos);
    } else {
        v.display_name = name;
    }

    auto [ins, _] = vertices_.emplace(name, v);
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

std::string Graph::getDisplayName(const std::string& internal_key) const {
    auto it = vertices_.find(internal_key);
    if (it != vertices_.end() && !it->second.display_name.empty())
        return it->second.display_name;
    return internal_key;
}

std::string Graph::resolveVertexName(const std::string& displayOrKey) const {
    // 精确匹配 internal key
    if (vertices_.count(displayOrKey))
        return displayOrKey;
    // 按 display_name 搜索
    for (const auto& [key, v] : vertices_) {
        if (v.display_name == displayOrKey)
            return key;
    }
    return displayOrKey; // fallback
}

bool Graph::addEdge(const std::string& from, const std::string& to,
                     double weight, bool directed, bool explicit_w)
{
    addVertex(from);
    addVertex(to);

    Edge e;
    e.from = from;
    e.to = to;
    e.weight = weight;
    e.directed = directed;
    e.id = edge_id_counter_++;
    e.explicit_weight = explicit_w;

    if (explicit_w)
        has_explicit_weight_ = true;

    bool isSelfLoop = (from == to);

    if (directed) {
        has_directed_ = true;
        adj_[from].push_back(e);
    } else {
        adj_[from].push_back(e);
        if (!isSelfLoop) {
            Edge rev = e;
            rev.from = to;
            rev.to = from;
            // 反边保持相同 id，确保 getAllEdges 去重
            adj_[to].push_back(rev);
        }
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
    // 按 id 去重：无向边的正反向共享同一 id，只收集一次
    // 平行边各有唯一 id，全部保留
    std::unordered_set<int> seen;
    std::vector<Edge> edges;
    edges.reserve(edge_id_counter_);

    for (const auto& kv : adj_) {
        for (const auto& e : kv.second) {
            if (seen.find(e.id) == seen.end()) {
                seen.insert(e.id);
                edges.push_back(e);
            }
        }
    }
    return edges;
}

size_t Graph::edgeCount() const {
    return getAllEdges().size();
}

size_t Graph::degree(const std::string& name) const {
    auto it = adj_.find(name);
    if (it == adj_.end()) return 0;
    size_t d = 0;
    for (const auto& e : it->second) {
        if (e.from == e.to) d += 2;
        else ++d;
    }
    return d;
}

size_t Graph::inDegree(const std::string& name) const {
    size_t count = 0;
    for (const auto& kv : adj_) {
        for (const auto& e : kv.second) {
            if (e.to == name) {
                if (e.directed) ++count;
                else if (kv.first != name) ++count;
                else count += 2;  // undirected self-loop
            }
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
    seen_edge_ids_.clear();
    next_id_ = 1;
    edge_id_counter_ = 1;
    has_directed_ = false;
    has_explicit_weight_ = false;
}
