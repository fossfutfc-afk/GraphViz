#include "GraphAlgorithm.h"

#include <algorithm>
#include <cmath>
#include <climits>
#include <queue>
#include <stack>
#include <unordered_map>
#include <unordered_set>

// ═══════════════════════════════════════════════════════════════
//  内部辅助
// ═══════════════════════════════════════════════════════════════

namespace {

/// 标准化边键 "from|to"（有向保持顺序，无向已由 Edge::makeKey 保证）
std::string edgeKey(const std::string& a, const std::string& b) {
    return a + "|" + b;
}

/// 无向标准化边键 (min|max)
std::string undirectedKey(const std::string& a, const std::string& b) {
    return (a <= b) ? (a + "|" + b) : (b + "|" + a);
}

} // anonymous namespace

// ═══════════════════════════════════════════════════════════════
//  连通分量（BFS — 兼容有向图底层，将无向边视为双向遍历）
// ═══════════════════════════════════════════════════════════════

ComponentResult GraphAlgorithm::connectedComponents(const Graph& graph)
{
    ComponentResult result;
    auto allNodes = graph.getAllVertexNames();
    std::unordered_set<std::string> visited;

    for (const auto& start : allNodes) {
        if (visited.count(start)) continue;

        std::vector<std::string> comp;
        std::queue<std::string> q;
        q.push(start);
        visited.insert(start);

        while (!q.empty()) {
            std::string u = q.front(); q.pop();
            comp.push_back(u);

            for (const auto& e : graph.getAdjacent(u)) {
                // 有向图只沿出边方向遍历
                // 无向图邻接表已包含双向，直接遍历即可
                std::string v = e.to;
                if (!visited.count(v)) {
                    visited.insert(v);
                    q.push(v);
                }
            }

            // 有向图：还需要沿入边方向遍历
            if (graph.hasDirectedEdges()) {
                for (const auto& vname : graph.getAllVertexNames()) {
                    if (vname == u) continue;
                    for (const auto& e2 : graph.getAdjacent(vname)) {
                        if (e2.directed && e2.to == u && !visited.count(vname)) {
                            visited.insert(vname);
                            q.push(vname);
                        }
                    }
                }
            }
        }

        if (!comp.empty())
            result.components.push_back(comp);
    }

    // 按大小降序排列
    std::sort(result.components.begin(), result.components.end(),
              [](const auto& a, const auto& b) { return a.size() > b.size(); });

    return result;
}

// ═══════════════════════════════════════════════════════════════
//  强连通分量（Kosaraju） — 仅对无向图有意义时当作连通分量
// ═══════════════════════════════════════════════════════════════

ComponentResult GraphAlgorithm::stronglyConnectedComponents(const Graph& graph)
{
    if (!graph.hasDirectedEdges()) {
        // 无向图：SCC 等同于连通分量
        return connectedComponents(graph);
    }

    ComponentResult result;
    auto allNodes = graph.getAllVertexNames();
    if (allNodes.empty()) return result;

    std::unordered_map<std::string, int> index;
    for (size_t i = 0; i < allNodes.size(); ++i)
        index[allNodes[i]] = static_cast<int>(i);

    int n = static_cast<int>(allNodes.size());

    // 构建正向图和反向图
    std::vector<std::vector<int>> g(n), rg(n);
    for (const auto& name : allNodes) {
        for (const auto& e : graph.getAdjacent(name)) {
            if (e.directed) {
                int u = index[e.from], v = index[e.to];
                g[u].push_back(v);
                rg[v].push_back(u);
            } else {
                int u = index[e.from], v = index[e.to];
                g[u].push_back(v);
                g[v].push_back(u);
                rg[u].push_back(v);
                rg[v].push_back(u);
            }
        }
    }

    // 第一遍 DFS：记录完成顺序
    std::vector<bool> visited(n, false);
    std::vector<int> order;

    std::function<void(int)> dfs1 = [&](int u) {
        visited[u] = true;
        for (int v : g[u])
            if (!visited[v]) dfs1(v);
        order.push_back(u);
    };

    for (int i = 0; i < n; ++i)
        if (!visited[i]) dfs1(i);

    // 第二遍 DFS：反向图
    std::fill(visited.begin(), visited.end(), false);

    std::function<void(int, std::vector<std::string>&)> dfs2 =
        [&](int u, std::vector<std::string>& comp) {
            visited[u] = true;
            comp.push_back(allNodes[u]);
            for (int v : rg[u])
                if (!visited[v]) dfs2(v, comp);
        };

    for (int i = n - 1; i >= 0; --i) {
        int u = order[i];
        if (!visited[u]) {
            std::vector<std::string> comp;
            dfs2(u, comp);
            result.components.push_back(comp);
        }
    }

    std::sort(result.components.begin(), result.components.end(),
              [](const auto& a, const auto& b) { return a.size() > b.size(); });

    return result;
}

// ═══════════════════════════════════════════════════════════════
//  最短路径（Dijkstra）
// ═══════════════════════════════════════════════════════════════

PathResult GraphAlgorithm::shortestPath(const Graph& graph,
                                         const std::string& from,
                                         const std::string& to)
{
    PathResult result;
    if (!graph.hasVertex(from) || !graph.hasVertex(to)) {
        result.found = false;
        return result;
    }
    if (from == to) {
        result.found = true;
        result.totalWeight = 0.0;
        result.nodes = {from};
        return result;
    }

    const double INF = 1e18;
    std::unordered_map<std::string, double> dist;
    std::unordered_map<std::string, std::string> prev;
    std::unordered_map<std::string, std::string> prevEdge;  // 记录入边键

    for (const auto& name : graph.getAllVertexNames())
        dist[name] = INF;
    dist[from] = 0.0;

    // pair<double, string> + greater 实现小根堆，确定性
    using PQItem = std::pair<double, std::string>;
    std::priority_queue<PQItem, std::vector<PQItem>, std::greater<PQItem>> pq;
    pq.push({0.0, from});

    while (!pq.empty()) {
        auto [d, u] = pq.top(); pq.pop();
        if (d > dist[u]) continue;
        if (u == to) break;

        for (const auto& e : graph.getAdjacent(u)) {
            std::string v = e.to;
            double nd = d + e.weight;
            // 无向边或有向出边都可以通过
            if (e.directed && e.from != u) continue; // 仅沿出边方向
            if (nd < dist[v]) {
                dist[v] = nd;
                prev[v] = u;
                prevEdge[v] = edgeKey(u, v);
                pq.push({nd, v});
            }
        }

        // 有向图额外检查入边（从 u 出发能到达的已在邻接表中，不需要额外处理）
    }

    if (dist[to] >= INF / 2) {
        result.found = false;
        return result;
    }

    result.found = true;
    result.totalWeight = dist[to];

    // 回溯路径
    std::vector<std::string> pathNodes;
    std::vector<std::string> pathEdges;
    std::string cur = to;
    while (cur != from) {
        pathNodes.push_back(cur);
        std::string p = prev[cur];
        pathEdges.push_back(undirectedKey(p, cur));
        cur = p;
    }
    pathNodes.push_back(from);
    std::reverse(pathNodes.begin(), pathNodes.end());
    std::reverse(pathEdges.begin(), pathEdges.end());

    result.nodes = pathNodes;
    result.edges = pathEdges;

    return result;
}

// ═══════════════════════════════════════════════════════════════
//  最小生成树（Kruskal — 无向图）
// ═══════════════════════════════════════════════════════════════

MSTResult GraphAlgorithm::minimumSpanningTree(const Graph& graph)
{
    MSTResult result;
    auto allNodes = graph.getAllVertexNames();
    if (allNodes.empty()) {
        result.connected = false;
        return result;
    }

    // 收集所有无向边（去重）
    std::vector<Edge> edges;
    std::unordered_set<std::string> seen;
    for (const auto& name : allNodes) {
        for (const auto& e : graph.getAdjacent(name)) {
            std::string key = undirectedKey(e.from, e.to);
            if (seen.count(key)) continue;
            seen.insert(key);
            Edge ee = e;
            // 统一方向
            if (ee.from > ee.to) std::swap(ee.from, ee.to);
            edges.push_back(ee);
        }
    }

    // 按权重排序
    std::sort(edges.begin(), edges.end(),
              [](const Edge& a, const Edge& b) { return a.weight < b.weight; });

    // DSU
    std::unordered_map<std::string, std::string> parent;
    std::unordered_map<std::string, int> rank;
    for (const auto& name : allNodes) {
        parent[name] = name;
        rank[name] = 0;
    }

    std::function<std::string(const std::string&)> find =
        [&](const std::string& x) -> std::string {
            if (parent[x] != x)
                parent[x] = find(parent[x]);
            return parent[x];
        };

    auto unite = [&](const std::string& a, const std::string& b) {
        std::string ra = find(a), rb = find(b);
        if (ra == rb) return false;
        if (rank[ra] < rank[rb]) parent[ra] = rb;
        else if (rank[ra] > rank[rb]) parent[rb] = ra;
        else { parent[rb] = ra; rank[ra]++; }
        return true;
    };

    for (const auto& e : edges) {
        if (unite(e.from, e.to)) {
            result.edges.push_back(e.from + "|" + e.to);
            result.totalWeight += e.weight;
        }
    }

    // 检查连通性
    std::string root = find(allNodes[0]);
    for (const auto& name : allNodes) {
        if (find(name) != root) {
            result.connected = false;
            return result;
        }
    }

    result.connected = true;
    return result;
}

// ═══════════════════════════════════════════════════════════════
//  关节点 & 桥（Tarjan DFS）
// ═══════════════════════════════════════════════════════════════

CriticalResult GraphAlgorithm::findCritical(const Graph& graph)
{
    CriticalResult result;
    auto allNodes = graph.getAllVertexNames();
    if (allNodes.empty()) return result;

    std::unordered_map<std::string, int> disc, low;
    std::unordered_map<std::string, bool> visited;
    std::unordered_set<std::string> artPoints;
    int time = 0;

    for (const auto& name : allNodes) {
        disc[name] = -1;
        low[name] = -1;
        visited[name] = false;
    }

    std::function<void(const std::string&, const std::string&)> dfs =
        [&](const std::string& u, const std::string& parent) {
            visited[u] = true;
            disc[u] = low[u] = ++time;
            int children = 0;

            for (const auto& e : graph.getAdjacent(u)) {
                std::string v = e.to;

                // 有向边可能不是双向的，需要特殊处理
                // 对于关节点/桥，我们只看无向连通性
                if (!visited[v]) {
                    ++children;
                    dfs(v, u);
                    low[u] = std::min(low[u], low[v]);

                    // 桥：low[v] > disc[u]
                    if (low[v] > disc[u]) {
                        result.bridges.push_back({u, v});
                    }

                    // 关节点（非根）：low[v] >= disc[u]
                    if (!parent.empty() && low[v] >= disc[u]) {
                        artPoints.insert(u);
                    }
                } else if (v != parent) {
                    // 回边
                    low[u] = std::min(low[u], disc[v]);
                }
            }

            // 根节点：children > 1 则是关节点
            if (parent.empty() && children > 1) {
                artPoints.insert(u);
            }
        };

    for (const auto& name : allNodes) {
        if (!visited[name]) {
            dfs(name, "");
        }
    }

    for (const auto& ap : artPoints)
        result.articulationPoints.push_back(ap);

    // 保持确定性排序
    std::sort(result.articulationPoints.begin(), result.articulationPoints.end());
    std::sort(result.bridges.begin(), result.bridges.end(),
              [](const auto& a, const auto& b) {
                  if (a.first != b.first) return a.first < b.first;
                  return a.second < b.second;
              });

    return result;
}

// ═══════════════════════════════════════════════════════════════
//  欧拉回路（Hierholzer — 无向图）
// ═══════════════════════════════════════════════════════════════

EulerResult GraphAlgorithm::eulerCircuit(const Graph& graph)
{
    EulerResult result;
    auto allNodes = graph.getAllVertexNames();
    if (allNodes.empty()) return result;

    // 检查条件：所有顶点度数为偶数，图连通（忽略孤立点）
    // 收集非零度顶点
    std::vector<std::string> nonZeroNodes;
    for (const auto& name : allNodes) {
        size_t deg = graph.degree(name);
        if (graph.hasDirectedEdges()) {
            // 有向图欧拉回路：每个点入度==出度
            size_t inDeg = graph.inDegree(name);
            size_t outDeg = 0;
            for (const auto& e : graph.getAdjacent(name))
                if (e.directed && e.from == name) ++outDeg;
            if (inDeg != outDeg) {
                result.exists = false;
                result.isCircuit = false;
                return result;
            }
            if (inDeg > 0 || outDeg > 0)
                nonZeroNodes.push_back(name);
        } else {
            if (deg % 2 != 0) {
                result.exists = false;
                result.isCircuit = false;
                return result;
            }
            if (deg > 0)
                nonZeroNodes.push_back(name);
        }
    }

    if (nonZeroNodes.empty()) {
        // 全为孤立点
        result.exists = false;
        return result;
    }

    // 检查连通性
    ComponentResult comp = connectedComponents(graph);
    int nonIsolatedComps = 0;
    for (const auto& c : comp.components) {
        if (c.size() > 1 || graph.degree(c[0]) > 0)
            ++nonIsolatedComps;
    }
    if (nonIsolatedComps > 1) {
        result.exists = false;
        return result;
    }

    // Hierholzer 算法
    // 构建可删除的邻接表（使用 multiset 或 vector + index）
    using AdjList = std::vector<std::pair<std::string, bool>>; // (to, removed)
    std::unordered_map<std::string, AdjList> adjCopy;

    for (const auto& name : allNodes) {
        for (const auto& e : graph.getAdjacent(name)) {
            adjCopy[name].push_back({e.to, false});
        }
    }

    std::string start = nonZeroNodes[0];
    std::vector<std::string> circuit;
    std::stack<std::string> stk;
    stk.push(start);

    while (!stk.empty()) {
        std::string u = stk.top();
        bool found = false;

        auto& neighbors = adjCopy[u];
        for (auto& pr : neighbors) {
            if (!pr.second) {
                // 无向边需要同时删除反向边
                std::string v = pr.first;
                pr.second = true;

                // 删除反向边
                auto& revNeighbors = adjCopy[v];
                for (auto& rev : revNeighbors) {
                    if (!rev.second && rev.first == u) {
                        rev.second = true;
                        break;
                    }
                }

                stk.push(v);
                found = true;
                break;
            }
        }

        if (!found) {
            stk.pop();
            circuit.push_back(u);
        }
    }

    // circuit 是逆序的
    std::reverse(circuit.begin(), circuit.end());

    result.exists = true;
    result.isCircuit = true;
    result.nodes = circuit;
    return result;
}

// ═══════════════════════════════════════════════════════════════
//  欧拉通路（Hierholzer 变体 — 无向图）
// ═══════════════════════════════════════════════════════════════

EulerResult GraphAlgorithm::eulerTrail(const Graph& graph)
{
    EulerResult result;

    // 先尝试回路
    EulerResult circuit = eulerCircuit(graph);
    if (circuit.exists) {
        circuit.isCircuit = true;
        return circuit;
    }

    auto allNodes = graph.getAllVertexNames();
    if (allNodes.empty()) return result;

    // 检查条件：恰有 0 或 2 个奇度顶点
    std::vector<std::string> oddNodes;
    std::vector<std::string> nonZeroNodes;
    for (const auto& name : allNodes) {
        size_t deg = graph.degree(name);
        if (deg > 0) nonZeroNodes.push_back(name);
        if (deg % 2 != 0) oddNodes.push_back(name);
    }

    if (oddNodes.size() != 2) {
        // 不是0也不是2 → 尝试作为欧拉通路也不存在
        // 但刚才回路检查已排除0的情况，所以这里只能是 != 2
        if (oddNodes.size() != 0) {
            result.exists = false;
            return result;
        }
    }

    // 如果 oddNodes 是 0（刚才回路失败但度全为偶，可能不连通）
    if (oddNodes.empty()) {
        result.exists = false;
        return result;
    }

    // 检查连通性
    ComponentResult comp = connectedComponents(graph);
    int nonIsolatedComps = 0;
    for (const auto& c : comp.components) {
        if (c.size() > 1 || (c.size() == 1 && graph.degree(c[0]) > 0))
            ++nonIsolatedComps;
    }
    if (nonIsolatedComps > 1) {
        result.exists = false;
        return result;
    }

    // Hierholzer 从奇度点出发
    std::string start = oddNodes[0];

    using AdjList = std::vector<std::pair<std::string, bool>>;
    std::unordered_map<std::string, AdjList> adjCopy;

    for (const auto& name : allNodes) {
        for (const auto& e : graph.getAdjacent(name)) {
            adjCopy[name].push_back({e.to, false});
        }
    }

    std::vector<std::string> trail;
    std::stack<std::string> stk;
    stk.push(start);

    while (!stk.empty()) {
        std::string u = stk.top();
        bool found = false;

        auto& neighbors = adjCopy[u];
        for (auto& pr : neighbors) {
            if (!pr.second) {
                std::string v = pr.first;
                pr.second = true;

                auto& revNeighbors = adjCopy[v];
                for (auto& rev : revNeighbors) {
                    if (!rev.second && rev.first == u) {
                        rev.second = true;
                        break;
                    }
                }

                stk.push(v);
                found = true;
                break;
            }
        }

        if (!found) {
            stk.pop();
            trail.push_back(u);
        }
    }

    std::reverse(trail.begin(), trail.end());

    result.exists = true;
    result.isCircuit = false;
    result.nodes = trail;
    return result;
}

// ═══════════════════════════════════════════════════════════════
//  哈密顿回路（回溯 + 剪枝）
// ═══════════════════════════════════════════════════════════════

HamiltonResult GraphAlgorithm::hamiltonCircuit(const Graph& graph)
{
    HamiltonResult result;
    result.isCircuit = true;

    auto allNodes = graph.getAllVertexNames();
    int n = static_cast<int>(allNodes.size());

    if (n == 0) {
        result.found = false;
        result.message = "图没有顶点";
        return result;
    }
    if (n == 1) {
        result.found = true;
        result.nodes = {allNodes[0]};
        return result;
    }
    if (n > 20) {
        result.found = false;
        result.message = "顶点数超过20，回溯计算太慢，已跳过";
        return result;
    }

    // 构建邻接矩阵（用于快速查找）
    std::unordered_map<std::string, int> idx;
    for (int i = 0; i < n; ++i) idx[allNodes[i]] = i;

    std::vector<std::vector<bool>> adj(n, std::vector<bool>(n, false));
    for (int i = 0; i < n; ++i) {
        for (const auto& e : graph.getAdjacent(allNodes[i])) {
            int j = idx[e.to];
            adj[i][j] = true;
            // 无向边反向也可达
            if (!e.directed)
                adj[j][i] = true;
        }
        // 有向图：检查入边
        if (graph.hasDirectedEdges()) {
            for (int k = 0; k < n; ++k) {
                if (k == i) continue;
                for (const auto& e : graph.getAdjacent(allNodes[k])) {
                    if (e.directed && e.to == allNodes[i])
                        adj[k][i] = true;
                }
            }
        }
    }

    std::vector<int> path;
    path.reserve(n + 1);
    std::vector<bool> visited(n, false);

    bool found = false;
    std::vector<int> bestPath;

    // 固定从顶点0开始（回路可从任意点开始）
    path.push_back(0);
    visited[0] = true;

    std::function<void()> backtrack = [&]() {
        if (found) return;

        if (static_cast<int>(path.size()) == n) {
            // 检查是否能回到起点
            if (adj[path.back()][0]) {
                found = true;
                bestPath = path;
                bestPath.push_back(0);
            }
            return;
        }

        int u = path.back();
        for (int v = 0; v < n; ++v) {
            if (!visited[v] && adj[u][v]) {
                visited[v] = true;
                path.push_back(v);
                backtrack();
                path.pop_back();
                visited[v] = false;
            }
        }
    };

    backtrack();

    if (found) {
        result.found = true;
        for (int v : bestPath)
            result.nodes.push_back(allNodes[v]);
    } else {
        result.found = false;
        result.message = "不存在哈密顿回路";
    }

    return result;
}

// ═══════════════════════════════════════════════════════════════
//  哈密顿通路（回溯 + 剪枝）
// ═══════════════════════════════════════════════════════════════

HamiltonResult GraphAlgorithm::hamiltonPath(const Graph& graph)
{
    HamiltonResult result;
    result.isCircuit = false;

    auto allNodes = graph.getAllVertexNames();
    int n = static_cast<int>(allNodes.size());

    if (n == 0) {
        result.found = false;
        result.message = "图没有顶点";
        return result;
    }
    if (n == 1) {
        result.found = true;
        result.nodes = {allNodes[0]};
        return result;
    }
    if (n > 20) {
        result.found = false;
        result.message = "顶点数超过20，回溯计算太慢，已跳过";
        return result;
    }

    // 构建邻接矩阵
    std::unordered_map<std::string, int> idx;
    for (int i = 0; i < n; ++i) idx[allNodes[i]] = i;

    std::vector<std::vector<bool>> adj(n, std::vector<bool>(n, false));
    for (int i = 0; i < n; ++i) {
        for (const auto& e : graph.getAdjacent(allNodes[i])) {
            int j = idx[e.to];
            adj[i][j] = true;
            if (!e.directed)
                adj[j][i] = true;
        }
        if (graph.hasDirectedEdges()) {
            for (int k = 0; k < n; ++k) {
                if (k == i) continue;
                for (const auto& e : graph.getAdjacent(allNodes[k])) {
                    if (e.directed && e.to == allNodes[i])
                        adj[k][i] = true;
                }
            }
        }
    }

    std::vector<int> path;
    path.reserve(n);
    std::vector<bool> visited(n, false);
    bool found = false;
    std::vector<int> bestPath;

    std::function<void()> backtrack = [&]() {
        if (found) return;

        if (static_cast<int>(path.size()) == n) {
            found = true;
            bestPath = path;
            return;
        }

        int u = path.back();
        for (int v = 0; v < n; ++v) {
            if (!visited[v] && adj[u][v]) {
                visited[v] = true;
                path.push_back(v);
                backtrack();
                path.pop_back();
                visited[v] = false;
            }
        }
    };

    // 尝试每个顶点作为起点
    for (int start = 0; start < n && !found; ++start) {
        path.clear();
        std::fill(visited.begin(), visited.end(), false);
        path.push_back(start);
        visited[start] = true;
        backtrack();
    }

    if (found) {
        result.found = true;
        for (int v : bestPath)
            result.nodes.push_back(allNodes[v]);
    } else {
        result.found = false;
        result.message = "不存在哈密顿通路";
    }

    return result;
}
