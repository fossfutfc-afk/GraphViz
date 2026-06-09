#include "GraphParser.h"

#include <cctype>
#include <regex>
#include <sstream>

// ═══════════════════════════════════════════════════════════════
//  辅助：行预处理
// ═══════════════════════════════════════════════════════════════

static std::string trimLine(const std::string& line) {
    // 去除 # 之后的注释内容
    size_t comment = line.find('#');
    std::string s = (comment != std::string::npos)
        ? line.substr(0, comment) : line;

    // 去除首尾空白
    size_t start = s.find_first_not_of(" \t\r");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r");
    return s.substr(start, end - start + 1);
}

// ═══════════════════════════════════════════════════════════════
//  辅助：引号内转义处理
// ═══════════════════════════════════════════════════════════════

static std::string unescape(const std::string& raw) {
    std::string result;
    result.reserve(raw.size());
    for (size_t i = 0; i < raw.size(); ++i) {
        if (raw[i] == '\\' && i + 1 < raw.size()) {
            char next = raw[i + 1];
            if (next == '"' || next == '\\') {
                result += next;
                ++i;
            } else {
                result += raw[i];  // 保留未知转义
            }
        } else {
            result += raw[i];
        }
    }
    return result;
}

// ═══════════════════════════════════════════════════════════════
//  辅助：判断顶点名是否需要加引号保存
// ═══════════════════════════════════════════════════════════════

static bool needsQuoting(const std::string& name) {
    if (name.empty()) return true;
    for (char c : name) {
        if (c == ' ' || c == '"' || c == '#') return true;
    }
    return false;
}

static std::string quoteName(const std::string& name) {
    std::string out = "\"";
    for (char c : name) {
        if (c == '"' || c == '\\') out += '\\';
        out += c;
    }
    out += '"';
    return out;
}

// ═══════════════════════════════════════════════════════════════
//  顶点名解析
// ═══════════════════════════════════════════════════════════════

/// 解析顶点名，返回 (name, newPos)。name 为空表示解析失败
static std::pair<std::string, size_t>
parseVertexName(const std::string& line, size_t pos) {
    // 跳过前导空白
    while (pos < line.size() && (line[pos] == ' ' || line[pos] == '\t'))
        ++pos;
    if (pos >= line.size()) return {"", pos};

    if (line[pos] == '"') {
        // 引号名
        ++pos;  // 跳过开始 "
        std::string raw;
        while (pos < line.size()) {
            if (line[pos] == '\\' && pos + 1 < line.size()) {
                raw += line[pos];
                raw += line[pos + 1];
                pos += 2;
            } else if (line[pos] == '"') {
                ++pos;  // 跳过结束 "
                return {unescape(raw), pos};
            } else {
                raw += line[pos];
                ++pos;
            }
        }
        // 未闭合引号 — 返回已读内容
        return {unescape(raw), pos};
    } else {
        // 非引号名：读取直到空白或操作符起始符 (- 或 <)
        size_t start = pos;
        while (pos < line.size()) {
            char c = line[pos];
            if (c == ' ' || c == '\t') break;
            // 遇到 - 或 <：需判断是否为操作符起始
            // 规则：如果前面没有紧邻空白且当前字符是 - 或 <，则回退
            if (c == '-' || c == '<') {
                // 检查这是否像操作符起始
                // 若前一个字符不是空白且当前是 -/<，可能是名称的一部分
                // 策略：检查后面是否跟着 -/> 以确认是操作符
                if (pos + 1 < line.size()) {
                    char next = line[pos + 1];
                    if (next == '-' || next == '>' || next == '<' ||
                        std::isdigit(static_cast<unsigned char>(next))) {
                        // 像操作符：- 或 < 后紧跟 -/>/<\\d
                        break;
                    }
                }
                // 单独一个 - 在行尾：也视为操作符
                if (pos + 1 >= line.size()) break;
            }
            // 普通字符：允许字母、数字、Unicode（高位字节）、常见符号
            // 排除 # 和 " 以避免干扰
            if (c == '#') break;
            ++pos;
        }
        if (pos == start) return {"", pos};
        return {line.substr(start, pos - start), pos};
    }
}

// ═══════════════════════════════════════════════════════════════
//  操作符解析
// ═══════════════════════════════════════════════════════════════

/// 解析边操作符，返回 (opEnd, directed, weight)。opEnd < 0 表示失败
static std::tuple<size_t, bool, double>
parseOperator(const std::string& line, size_t pos) {
    // 跳过前导空白
    while (pos < line.size() && (line[pos] == ' ' || line[pos] == '\t'))
        ++pos;
    if (pos >= line.size()) return {std::string::npos, false, 1.0};

    // 操作符必须以 - 或 < 开始
    char first = line[pos];
    if (first != '-' && first != '<')
        return {std::string::npos, false, 1.0};

    // 收集操作符全部字符：[-\d.<>]+
    size_t start = pos;
    std::string opChars;
    while (pos < line.size()) {
        char c = line[pos];
        if (c == '-' || c == '<' || c == '>' || c == '.' ||
            std::isdigit(static_cast<unsigned char>(c))) {
            opChars += c;
            ++pos;
        } else {
            break;
        }
    }

    if (opChars.empty()) return {std::string::npos, false, 1.0};

    // ── 分析操作符 ──
    bool directed = false;
    bool hasBidi = false;
    double weight = 1.0;

    for (char c : opChars) {
        if (c == '>') directed = true;
        if (c == '<') hasBidi = true;
    }

    // 若同时含有 < 和 >，视为无向（bidirectional → undirected）
    if (directed && hasBidi) directed = false;

    // 提取权重数字
    std::string numStr;
    bool inNum = false;
    for (size_t i = 0; i < opChars.size(); ++i) {
        char c = opChars[i];
        if (std::isdigit(static_cast<unsigned char>(c)) || c == '.') {
            if (!inNum) { inNum = true; numStr.clear(); }
            numStr += c;
        } else {
            if (inNum) {
                // 遇到第一个非数字结束数字提取
                break;
            }
        }
    }
    if (!numStr.empty()) {
        try {
            weight = std::stod(numStr);
        } catch (...) { weight = 1.0; }
    }

    return {pos, directed, weight};
}

// ═══════════════════════════════════════════════════════════════
//  公开接口
// ═══════════════════════════════════════════════════════════════

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

        line = trimLine(line);
        if (line.empty()) continue;

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
    // 1. 解析左顶点
    auto [left, pos] = parseVertexName(line, 0);
    if (left.empty()) {
        error = "Missing left vertex name";
        return false;
    }

    // 2. 解析操作符
    auto [opEnd, directed, weight] = parseOperator(line, pos);
    if (opEnd == std::string::npos) {
        error = "Cannot parse edge operator after \"" + left + "\"";
        return false;
    }

    // 3. 解析右顶点
    auto [right, _] = parseVertexName(line, opEnd);
    if (right.empty()) {
        error = "Missing right vertex name after operator";
        return false;
    }

    // 4. 添加边
    graph.addEdge(left, right, weight, directed);
    return true;
}

std::string GraphParser::serialize(const Graph& graph)
{
    std::ostringstream oss;
    auto edges = graph.getAllEdges();
    for (const auto& e : edges) {
        std::string fromStr = needsQuoting(e.from) ? quoteName(e.from) : e.from;
        std::string toStr   = needsQuoting(e.to)   ? quoteName(e.to)   : e.to;

        if (e.from == e.to) {
            // 自环
            if (e.directed) {
                if (e.weight == 1.0)
                    oss << fromStr << "-->- " << toStr << "\n";
                else
                    oss << fromStr << "-" << e.weight << "->- " << toStr << "\n";
            } else {
                if (e.weight == 1.0)
                    oss << fromStr << "----- " << toStr << "\n";
                else
                    oss << fromStr << "-" << e.weight << "-- " << toStr << "\n";
            }
            continue;
        }

        if (e.directed) {
            if (e.weight == 1.0)
                oss << fromStr << "--> " << toStr << "\n";
            else if (e.weight == static_cast<int>(e.weight))
                oss << fromStr << "-" << static_cast<int>(e.weight) << "-> " << toStr << "\n";
            else
                oss << fromStr << "-" << e.weight << "-> " << toStr << "\n";
        } else {
            if (e.weight == 1.0)
                oss << fromStr << "--- " << toStr << "\n";
            else if (e.weight == static_cast<int>(e.weight))
                oss << fromStr << "-" << static_cast<int>(e.weight) << "-- " << toStr << "\n";
            else
                oss << fromStr << "-" << e.weight << "-- " << toStr << "\n";
        }
    }
    return oss.str();
}
