#ifndef GRAPHTYPES_H
#define GRAPHTYPES_H

#include <string>

/// 顶点 — 唯一标识为 name，display_name 为画布显示名
struct Vertex {
    std::string name;          // 内部唯一 key（如 "A" 或 "A#1"）
    std::string display_name;  // 画布显示名（如 "A"），空时回退到 name
    int id = 0;                // 自动分配编号
};

/// 边 — 有向/无向、带权/无权重
struct Edge {
    std::string from;
    std::string to;
    double weight = 1.0;
    bool directed = false;
    int id = 0;                // 唯一标识（平行边用）
    bool explicit_weight = false;  // 用户显式提供了权重

    /// 生成端点标准化键（不含 id）
    std::string makeKey() const;
};

#endif // GRAPHTYPES_H
