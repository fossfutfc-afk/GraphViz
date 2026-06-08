#include "GraphParser.h"

#include <regex>
#include <sstream>

Graph GraphParser::parse(const std::string& text,
                          std::vector<std::string>& errors)
{
    Graph graph;
    errors.clear();

    std::istringstream stream(text);
    std::string line;
    int lineNum = 0;

    while (std::getline(stream, line)) {
        ++lineNum;

        // 去除首尾空白
        size_t start = line.find_first_not_of(" \t\r");
        if (start == std::string::npos) continue;  // 空行
        size_t end = line.find_last_not_of(" \t\r");
        line = line.substr(start, end - start + 1);

        if (line.empty() || line[0] == '#') continue;  // 注释

        std::string error;
        if (!parseLine(line, graph, error)) {
            errors.push_back("Line " + std::to_string(lineNum) + ": " + error);
        }
    }

    return graph;
}

bool GraphParser::parseLine(const std::string& line, Graph& graph,
                             std::string& error)
{
    // 正则匹配四种格式
    // 格式1: a-->b  有向无权
    // 格式2: a-W->b 有向带权
    // 格式3: a---b  无向无权
    // 格式4: a-W--b 无向带权
    // 顶点名允许字母、数字、下划线、连字符、中文等

    // 匹配: 顶点名 - (权重) (-->/---/->/--)  顶点名
    // 具体来说:
    //   有向无权: ^([^\-]+)-->\s*(.+)$
    //   有向带权: ^([^\-]+)-(\d+(?:\.\d+)?)->\s*(.+)$
    //   无向无权: ^([^\-]+)---\s*(.+)$
    //   无向带权: ^([^\-]+)-(\d+(?:\.\d+)?)--\s*(.+)$

    // 尝试匹配带权有向边: name-W->name
    {
        std::regex re(R"(^(.+?)-(\d+(?:\.\d+)?)->(.+)$)");
        std::smatch m;
        if (std::regex_match(line, m, re)) {
            std::string from = m[1].str();
            std::string to = m[3].str();
            double weight = std::stod(m[2].str());
            graph.addEdge(from, to, weight, true);
            return true;
        }
    }

    // 尝试匹配无权有向边: name-->name
    {
        std::regex re(R"(^(.+?)-->(.+)$)");
        std::smatch m;
        if (std::regex_match(line, m, re)) {
            std::string from = m[1].str();
            std::string to = m[2].str();
            // 确保这不是带权格式的误匹配（如 a-2-3->b 这种非法格式）
            // 检查 from 是否以 "-数字" 结尾，如果是则说明可能格式错误
            graph.addEdge(from, to, 1.0, true);
            return true;
        }
    }

    // 尝试匹配带权无向边: name-W--name
    {
        std::regex re(R"(^(.+?)-(\d+(?:\.\d+)?)--(.+)$)");
        std::smatch m;
        if (std::regex_match(line, m, re)) {
            std::string from = m[1].str();
            std::string to = m[3].str();
            double weight = std::stod(m[2].str());
            graph.addEdge(from, to, weight, false);
            return true;
        }
    }

    // 尝试匹配无权无向边: name---name
    {
        std::regex re(R"(^(.+?)---(.+)$)");
        std::smatch m;
        if (std::regex_match(line, m, re)) {
            std::string from = m[1].str();
            std::string to = m[2].str();
            graph.addEdge(from, to, 1.0, false);
            return true;
        }
    }

    error = "Unrecognized format: \"" + line + "\"";
    return false;
}

std::string GraphParser::serialize(const Graph& graph)
{
    std::ostringstream oss;
    auto edges = graph.getAllEdges();
    for (const auto& e : edges) {
        if (e.directed) {
            if (e.weight == 1.0 || e.weight == static_cast<int>(e.weight)) {
                // 整数权重（含默认1.0）
                if (e.weight == 1.0)
                    oss << e.from << "-->" << e.to << "\n";
                else
                    oss << e.from << "-" << static_cast<int>(e.weight) << "->" << e.to << "\n";
            } else {
                oss << e.from << "-" << e.weight << "->" << e.to << "\n";
            }
        } else {
            if (e.weight == 1.0 || e.weight == static_cast<int>(e.weight)) {
                if (e.weight == 1.0)
                    oss << e.from << "---" << e.to << "\n";
                else
                    oss << e.from << "-" << static_cast<int>(e.weight) << "--" << e.to << "\n";
            } else {
                oss << e.from << "-" << e.weight << "--" << e.to << "\n";
            }
        }
    }
    return oss.str();
}
