#ifndef GRAPHPARSER_H
#define GRAPHPARSER_H

#include "Graph.h"

#include <string>
#include <vector>

/// 图数据文本解析器
/// 支持的格式（每行一条边）：
///   a-->b       有向无权边 (weight=1.0)
///   a-2.5->b    有向带权边
///   a---b       无向无权边 (weight=1.0)
///   a-3--b      无向带权边
/// 支持 # 开头的注释和空行
class GraphParser {
public:
    /// 从多行文本解析，构建并返回 Graph
    /// @param text  输入文本（支持 \\n 或 \\r\\n）
    /// @param errors  解析错误信息（输出参数）
    /// @return 解析后的图（即使有部分错误也会返回已解析的部分）
    static Graph parse(const std::string& text, std::vector<std::string>& errors);

    /// 将 Graph 序列化为文本格式
    static std::string serialize(const Graph& graph);

    /// 解析单行
    /// @return true 解析成功
    static bool parseLine(const std::string& line, Graph& graph,
                          std::string& error);
};

#endif // GRAPHPARSER_H
