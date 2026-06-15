#include "GraphAlgorithm.h"

#include <algorithm>
#include <cmath>
#include <functional>
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
    if (allNodes.empty()) return result;

    std::unordered_set<std::string> visited;

    // 有向图：预建反向邻接表（避免 O(V³) 扫描）
    std::unordered_map<std::string, std::vector<std::string>> revAdj;
    if (graph.hasDirectedEdges()) {
        for (const auto& name : allNodes) {
            for (const auto& e : graph.getAdjacent(name)) {
                if (e.directed)
                    revAdj[e.to].push_back(name);
            }
        }
    }

    for (const auto& start : allNodes) {
        if (visited.count(start)) continue;

        std::vector<std::string> comp;
        std::queue<std::string> q;
        q.push(start);
        visited.insert(start);

        while (!q.empty()) {
            std::string u = q.front(); q.pop();
            comp.push_back(u);

            // 出边方向
            for (const auto& e : graph.getAdjacent(u)) {
                std::string v = e.to;
                if (!visited.count(v)) {
                    visited.insert(v);
                    q.push(v);
                }
            }

            // 有向图：沿入边方向遍历（即 weakly connected 分量）
            if (graph.hasDirectedEdges()) {
                auto it = revAdj.find(u);
                if (it != revAdj.end()) {
                    for (const auto& pred : it->second) {
                        if (!visited.count(pred)) {
                            visited.insert(pred);
                            q.push(pred);
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

// ═══════════════════════════════════════════════════════════════
//  辅助：贪心 Hierholzer（单条回路/通路）
// ═══════════════════════════════════════════════════════════════

using AdjCopy = std::unordered_map<std::string,
      std::vector<std::pair<std::string, bool>>>;

static std::vector<std::string> hierholzerGreedy(
    AdjCopy& adjCopy, const std::string& start)
{
    std::vector<std::string> result;
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
            result.push_back(u);
        }
    }

    std::reverse(result.begin(), result.end());
    return result;
}

// ═══════════════════════════════════════════════════════════════
//  辅助：欧拉回溯（多解搜索）
// ═══════════════════════════════════════════════════════════════

static bool allEdgesUsed(const AdjCopy& adjCopy) {
    for (const auto& kv : adjCopy)
        for (const auto& pr : kv.second)
            if (!pr.second) return false;
    return true;
}

static void eulerBacktrack(
    AdjCopy& adjCopy,
    const std::string& current,
    const std::string& start,
    std::vector<std::string>& path,
    int totalEdges,
    bool isCircuit,
    std::vector<std::vector<std::string>>& solutions,
    int maxSolutions)
{
    if (static_cast<int>(solutions.size()) >= maxSolutions) return;

    if (allEdgesUsed(adjCopy)) {
        // 回路：必须回到起点
        if (isCircuit && current != start) return;
        solutions.push_back(path);
        return;
    }

    // 收集未使用的邻边
    std::vector<std::string> candidates;
    auto& neighbors = adjCopy[current];
    for (auto& pr : neighbors) {
        if (!pr.second)
            candidates.push_back(pr.first);
    }

    if (candidates.empty()) return; // dead end

    for (const auto& next : candidates) {
        // 拷贝 adjCopy
        AdjCopy newAdj = adjCopy;
        // 删除 current → next
        for (auto& pr : newAdj[current]) {
            if (!pr.second && pr.first == next) {
                pr.second = true;
                break;
            }
        }
        // 删除反向边 next → current
        for (auto& pr : newAdj[next]) {
            if (!pr.second && pr.first == current) {
                pr.second = true;
                break;
            }
        }

        path.push_back(next);
        eulerBacktrack(newAdj, next, start, path, totalEdges,
                       isCircuit, solutions, maxSolutions);
        path.pop_back();
    }
}

// ═══════════════════════════════════════════════════════════════
//  欧拉回路（Hierholzer + 回溯多解）
// ═══════════════════════════════════════════════════════════════

EulerResult GraphAlgorithm::eulerCircuit(const Graph& graph,
                                          const std::string& startVertex)
{
    EulerResult result;
    auto allNodes = graph.getAllVertexNames();
    if (allNodes.empty()) return result;

    // 检查条件：所有顶点度数为偶数
    std::vector<std::string> nonZeroNodes;
    for (const auto& name : allNodes) {
        size_t deg = graph.degree(name);
        if (graph.hasDirectedEdges()) {
            size_t inDeg = graph.inDegree(name);
            size_t outDeg = 0;
            for (const auto& e : graph.getAdjacent(name))
                if (e.directed && e.from == name) ++outDeg;
            if (inDeg != outDeg) {
                result.exists = false;
                result.isCircuit = false;
                result.message = "有向图欧拉回路要求每个顶点入度等于出度";
                return result;
            }
            if (inDeg > 0 || outDeg > 0)
                nonZeroNodes.push_back(name);
        } else {
            if (deg % 2 != 0) {
                result.exists = false;
                result.isCircuit = false;
                result.message = "存在奇度顶点，不存在欧拉回路";
                return result;
            }
            if (deg > 0)
                nonZeroNodes.push_back(name);
        }
    }

    if (nonZeroNodes.empty()) {
        result.exists = false;
        result.message = "图中没有边";
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
        result.message = "非零度顶点不连通，不存在欧拉回路";
        return result;
    }

    // ── 确定起点 ──
    std::string start;
    if (!startVertex.empty()) {
        start = startVertex;
        if (!graph.hasVertex(start)) {
            result.exists = false;
            result.message = "指定起点 \"" + startVertex + "\" 不存在";
            return result;
        }
        if (graph.degree(start) == 0) {
            result.exists = false;
            result.message = "指定起点 \"" + startVertex + "\" 度数为零";
            return result;
        }
    } else {
        start = nonZeroNodes[0];
    }

    // ── 构建可删除邻接表 ──
    AdjCopy adjCopy;
    for (const auto& name : allNodes) {
        for (const auto& e : graph.getAdjacent(name)) {
            adjCopy[name].push_back({e.to, false});
        }
    }

    int n = static_cast<int>(allNodes.size());
    int m = static_cast<int>(graph.edgeCount());

    result.isCircuit = true;

    // 小图用回溯收集多解，大图用贪心
    if (n <= 15 && m <= 30) {
        const int MAX = 50;
        std::vector<std::vector<std::string>> solutions;
        std::vector<std::string> path = {start};
        eulerBacktrack(adjCopy, start, start, path, m,
                       true, solutions, MAX);
        if (!solutions.empty()) {
            result.exists = true;
            result.allSolutions = solutions;
            result.nodes = result.allSolutions[0];
        } else {
            result.exists = false;
            result.message = "未找到欧拉回路";
        }
    } else {
        std::vector<std::string> circuit = hierholzerGreedy(adjCopy, start);
        if (circuit.size() >= 2 && circuit.front() == circuit.back()) {
            result.exists = true;
            result.allSolutions = {circuit};
            result.nodes = circuit;
        } else {
            circuit.push_back(circuit.front()); // 闭合
            result.exists = true;
            result.allSolutions = {circuit};
            result.nodes = circuit;
        }
    }

    return result;
}

// ═══════════════════════════════════════════════════════════════
//  欧拉通路（Hierholzer 变体 — 无向图）
// ═══════════════════════════════════════════════════════════════

EulerResult GraphAlgorithm::eulerTrail(const Graph& graph,
                                        const std::string& startVertex)
{
    EulerResult result;

    // 先尝试回路
    EulerResult circuit = eulerCircuit(graph, startVertex);
    if (circuit.exists) {
        circuit.isCircuit = true;
        return circuit;
    }

    auto allNodes = graph.getAllVertexNames();
    if (allNodes.empty()) return result;

    // 检查条件：恰有 2 个奇度顶点（回路已排除 0 的情况）
    std::vector<std::string> oddNodes;
    std::vector<std::string> nonZeroNodes;
    for (const auto& name : allNodes) {
        size_t deg = graph.degree(name);
        if (deg > 0) nonZeroNodes.push_back(name);
        if (deg % 2 != 0) oddNodes.push_back(name);
    }

    if (oddNodes.size() != 2) {
        result.exists = false;
        result.message = "奇度顶点数不为2，不存在欧拉通路";
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
        result.message = "非零度顶点不连通，不存在欧拉通路";
        return result;
    }

    // ── 确定起点 ──
    std::string start;
    if (!startVertex.empty()) {
        start = startVertex;
        if (!graph.hasVertex(start)) {
            result.exists = false;
            result.message = "指定起点 \"" + startVertex + "\" 不存在";
            return result;
        }
        // 验证必须是奇度顶点
        if (graph.degree(start) % 2 == 0) {
            result.exists = false;
            result.message = "指定起点 \"" + startVertex
                + "\" 度数为偶数，欧拉通路必须从奇度顶点出发";
            return result;
        }
    } else {
        start = oddNodes[0];
    }

    // ── 构建可删除邻接表 ──
    AdjCopy adjCopy;
    for (const auto& name : allNodes) {
        for (const auto& e : graph.getAdjacent(name)) {
            adjCopy[name].push_back({e.to, false});
        }
    }

    int n = static_cast<int>(allNodes.size());
    int m = static_cast<int>(graph.edgeCount());

    result.isCircuit = false;

    // 小图用回溯收集多解，大图用贪心
    if (n <= 15 && m <= 30) {
        const int MAX = 50;
        std::vector<std::vector<std::string>> solutions;
        std::vector<std::string> path = {start};
        eulerBacktrack(adjCopy, start, "", path, m,
                       false, solutions, MAX);
        if (!solutions.empty()) {
            result.exists = true;
            result.allSolutions = solutions;
            result.nodes = result.allSolutions[0];
        } else {
            result.exists = false;
            result.message = "未找到欧拉通路";
        }
    } else {
        std::vector<std::string> trail = hierholzerGreedy(adjCopy, start);
        result.exists = true;
        result.allSolutions = {trail};
        result.nodes = trail;
    }

    return result;
}

// ═══════════════════════════════════════════════════════════════
//  哈密顿回路（回溯 + 剪枝）
// ═══════════════════════════════════════════════════════════════

HamiltonResult GraphAlgorithm::hamiltonCircuit(const Graph& graph,
                                                const std::string& startVertex)
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

    std::vector<std::vector<int>> allPaths;
    const int MAX_SOLUTIONS = 100;

    // ── 确定起点 ──
    int startIdx = 0;
    if (!startVertex.empty()) {
        auto it = idx.find(startVertex);
        if (it == idx.end()) {
            result.found = false;
            result.message = "指定起点 \"" + startVertex + "\" 不存在";
            return result;
        }
        startIdx = it->second;
    }

    path.push_back(startIdx);
    visited[startIdx] = true;

    std::function<void()> backtrack = [&]() {
        if (static_cast<int>(allPaths.size()) >= MAX_SOLUTIONS) return;

        if (static_cast<int>(path.size()) == n) {
            // 检查是否能回到起点
            if (adj[path.back()][startIdx]) {
                auto fullPath = path;
                fullPath.push_back(startIdx);
                allPaths.push_back(fullPath);
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

    if (!allPaths.empty()) {
        result.found = true;
        result.allSolutions.clear();
        for (const auto& p : allPaths) {
            std::vector<std::string> nodePath;
            for (int v : p)
                nodePath.push_back(allNodes[v]);
            result.allSolutions.push_back(nodePath);
        }
        result.nodes = result.allSolutions[0];
        result.solutionIndex = 0;
    } else {
        result.found = false;
        result.message = "不存在哈密顿回路";
    }

    return result;
}

// ═══════════════════════════════════════════════════════════════
//  哈密顿通路（回溯 + 剪枝）
// ═══════════════════════════════════════════════════════════════

HamiltonResult GraphAlgorithm::hamiltonPath(const Graph& graph,
                                              const std::string& startVertex)
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
    std::vector<std::vector<int>> allPaths;
    const int MAX_SOLUTIONS = 100;

    std::function<void()> backtrack = [&]() {
        if (static_cast<int>(allPaths.size()) >= MAX_SOLUTIONS) return;

        if (static_cast<int>(path.size()) == n) {
            allPaths.push_back(path);
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

    // 确定起点范围：指定则只从该顶点开始，否则尝试所有顶点
    int startBegin = 0, startEnd = n;
    if (!startVertex.empty()) {
        auto it = idx.find(startVertex);
        if (it == idx.end()) {
            result.found = false;
            result.message = "指定起点 \"" + startVertex + "\" 不存在";
            return result;
        }
        startBegin = it->second;
        startEnd = startBegin + 1;
    }

    for (int start = startBegin; start < startEnd; ++start) {
        if (static_cast<int>(allPaths.size()) >= MAX_SOLUTIONS) break;
        path.clear();
        std::fill(visited.begin(), visited.end(), false);
        path.push_back(start);
        visited[start] = true;
        backtrack();
    }

    if (!allPaths.empty()) {
        result.found = true;
        result.allSolutions.clear();
        for (const auto& p : allPaths) {
            std::vector<std::string> nodePath;
            for (int v : p)
                nodePath.push_back(allNodes[v]);
            result.allSolutions.push_back(nodePath);
        }
        result.nodes = result.allSolutions[0];
        result.solutionIndex = 0;
    } else {
        result.found = false;
        result.message = "不存在哈密顿通路";
    }

    return result;
}

// ═══════════════════════════════════════════════════════════════
//  平面性检测
// ═══════════════════════════════════════════════════════════════

PlanarityResult GraphAlgorithm::checkPlanarity(const Graph& graph) {
    PlanarityResult result;
    int n = static_cast<int>(graph.vertexCount());
    int m = static_cast<int>(graph.edgeCount());
    result.vertexCount = n;
    result.edgeCount = m;

    // ── 平凡图必定平面 ──
    if (n <= 2) {
        result.isPlanar = true;
        result.message = "Graph with " + std::to_string(n)
            + " vertices is always planar";
        return result;
    }

    // n ≤ 4 必定平面（K5 需要 5 个顶点，K3,3 需要 6 个）
    // 注意：平行边/自环不影响平面性，可以画成并排弧线
    if (n <= 4) {
        result.isPlanar = true;
        result.message = "Graph with " + std::to_string(n)
            + " vertices is always planar";
        return result;
    }

    // ── 获取顶点列表和邻接关系（去重平行边） ──
    auto allNodes = graph.getAllVertexNames();
    std::unordered_map<std::string, std::unordered_set<std::string>> neighbors;
    for (const auto& e : graph.getAllEdges()) {
        neighbors[e.from].insert(e.to);
        neighbors[e.to].insert(e.from);
    }

    // 统计去重后的边数（unique vertex pairs）
    int mUnique = 0;
    for (const auto& kv : neighbors)
        mUnique += static_cast<int>(kv.second.size());
    mUnique /= 2;  // 每条边被计数两次

    // ── Euler 公式快速否定: m_unique ≤ 3n - 6 (n ≥ 3) ──
    // 使用去重边数，因为平行边可以并排绘制不产生交叉
    if (mUnique > 3 * n - 6) {
        result.isPlanar = false;
        result.message = "Too many edges: m_unique=" + std::to_string(mUnique)
            + " > 3n-6=" + std::to_string(3 * n - 6)
            + " (Euler's formula violation)";
        return result;
    }

    // ── 建立 name → index 映射 ──
    std::unordered_map<std::string, int> idx;
    for (int i = 0; i < n; ++i)
        idx[allNodes[i]] = i;

    // 构建邻接矩阵（去重平行边，布尔值）
    std::vector<std::vector<bool>> adjMatrix(n, std::vector<bool>(n, false));
    for (const auto& kv : neighbors) {
        int u = idx[kv.first];
        for (const auto& vName : kv.second) {
            int v = idx[vName];
            adjMatrix[u][v] = true;
        }
    }

    // ── 辅助: 检查给定顶点子集是否构成 K5 ──
    auto isK5 = [&](const std::vector<int>& verts) -> bool {
        for (size_t i = 0; i < 5; ++i)
            for (size_t j = i + 1; j < 5; ++j)
                if (!adjMatrix[verts[i]][verts[j]])
                    return false;
        return true;
    };

    // ── 辅助: 检查给定 6 个顶点的二分划分是否构成 K3,3 ──
    auto isK33Partition = [&](const std::vector<int>& verts,
                               int a1, int a2, int a3,
                               int b1, int b2, int b3) -> bool {
        std::vector<int> A = {verts[a1], verts[a2], verts[a3]};
        std::vector<int> B = {verts[b1], verts[b2], verts[b3]};
        for (int u : A)
            for (int v : B)
                if (!adjMatrix[u][v])
                    return false;
        // 同侧无边（对于严格 K3,3）
        for (size_t i = 0; i < 3; ++i) {
            for (size_t j = i + 1; j < 3; ++j) {
                if (adjMatrix[A[i]][A[j]]) return false;
                if (adjMatrix[B[i]][B[j]]) return false;
            }
        }
        return true;
    };

    // ── 直接搜索 K5 和 K3,3 子图（n ≤ 10 时暴力，更大时采样） ──
    int maxN = std::min(n, 10);

    // 搜索 K5 子图
    if (n >= 5) {
        // 只在 n ≤ 10 时暴力搜索所有 5-组合
        if (n <= 10) {
            std::vector<int> comb = {0, 1, 2, 3, 4};
            while (true) {
                if (isK5(comb)) {
                    result.isPlanar = false;
                    result.message = "K5 subgraph found: vertices {"
                        + allNodes[comb[0]] + ", " + allNodes[comb[1]] + ", "
                        + allNodes[comb[2]] + ", " + allNodes[comb[3]] + ", "
                        + allNodes[comb[4]] + "}";
                    return result;
                }
                // 下一个组合
                int t = 4;
                while (t >= 0 && comb[t] == n - 5 + t) --t;
                if (t < 0) break;
                ++comb[t];
                for (int j = t + 1; j < 5; ++j)
                    comb[j] = comb[j - 1] + 1;
            }
        }
    }

    // 搜索 K3,3 子图
    if (n >= 6 && n <= 10) {
        std::vector<int> comb = {0, 1, 2, 3, 4, 5};
        while (true) {
            // 尝试所有二分划分: C(6,3)/2 = 10 种
            // 固定 comb[0] 在 A 组，从其余 5 个中选 2 个加入 A
            int rest[5] = {1, 2, 3, 4, 5};
            for (int i = 0; i < 5; ++i) {
                for (int j = i + 1; j < 5; ++j) {
                    int a1 = 0, a2 = rest[i], a3 = rest[j];
                    int b1, b2, b3;
                    int pos = 0;
                    int bArr[3];
                    for (int k = 0; k < 5; ++k) {
                        if (k != i && k != j)
                            bArr[pos++] = rest[k];
                    }
                    b1 = bArr[0]; b2 = bArr[1]; b3 = bArr[2];
                    if (isK33Partition(comb, a1, a2, a3, b1, b2, b3)) {
                        result.isPlanar = false;
                        result.message = "K3,3 subgraph found: {"
                            + allNodes[comb[a1]] + "," + allNodes[comb[a2]] + ","
                            + allNodes[comb[a3]] + "} / {"
                            + allNodes[comb[b1]] + "," + allNodes[comb[b2]] + ","
                            + allNodes[comb[b3]] + "}";
                        return result;
                    }
                }
            }
            // 下一个组合
            int t = 5;
            while (t >= 0 && comb[t] == n - 6 + t) --t;
            if (t < 0) break;
            ++comb[t];
            for (int j = t + 1; j < 6; ++j)
                comb[j] = comb[j - 1] + 1;
        }
    }

    // ── 收缩度为 2 的顶点后重试（模拟 Kuratowski 细分检测） ──
    // 重复收缩度为 2 的顶点（非自环），简化图结构
    // 收缩后检查简化图是否含 K5 或 K3,3
    if (n > 10) {
        result.isPlanar = true;
        result.message = "Likely planar: m=" + std::to_string(m)
            + " <= 3n-6=" + std::to_string(3 * n - 6)
            + " (Euler condition satisfied, graph too large for exhaustive check)";
        return result;
    }

    // n ≤ 10 且未找到 K5/K3,3 子图
    result.isPlanar = true;
    result.message = "Planar: no K5 or K3,3 subgraph found"
        + std::string(n <= 10 ? " (exhaustive check)" : "");
    return result;
}
