#ifndef GRAPHTYPES_H
#define GRAPHTYPES_H

#include <string>

/// 顶点 — 唯一标识为 name
struct Vertex {
    std::string name;
    int id = 0;          // 自动分配编号
};

/// 边 — 有向/无向、带权/无权重
struct Edge {
    std::string from;
    std::string to;
    double weight = 1.0;
    bool directed = false;

    /// 生成去重用标准化键
    /// 无向边: min|max ; 有向边: from|to
    std::string makeKey() const;
};

#endif // GRAPHTYPES_H
