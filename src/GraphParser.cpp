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

/// 解析边操作符，返回 (opEnd, directed, weight, explicit_weight, reverse)
/// reverse=true 表示方向为右→左（如 a<---b → b 指向 a）
/// 若 opEnd==npos 表示解析失败（error 中记录原因）
static std::tuple<size_t, bool, double, bool, bool>
parseOperator(const std::string& line, size_t pos, std::string& error) {
    error.clear();

    // 跳过前导空白
    while (pos < line.size() && (line[pos] == ' ' || line[pos] == '\t'))
        ++pos;
    if (pos >= line.size()) {
        error = "Missing edge operator";
        return {std::string::npos, false, 1.0, false, false};
    }

    // 操作符必须以 - 或 < 开始
    char first = line[pos];
    if (first != '-' && first != '<') {
        error = "Operator must start with '-' or '<'";
        return {std::string::npos, false, 1.0, false, false};
    }

    // 收集操作符字符：[-\d.<>] 以及引号包裹的权重
    std::string opChars;
    bool hasQuotedWeight = false;
    std::string quotedWeightStr;

    while (pos < line.size()) {
        char c = line[pos];
        if (c == '"') {
            // 引号包裹的权重（如 "-2"）
            ++pos;
            std::string quoted;
            while (pos < line.size() && line[pos] != '"') {
                quoted += line[pos];
                ++pos;
            }
            if (pos < line.size()) ++pos;  // 跳过闭合引号
            quotedWeightStr = quoted;
            hasQuotedWeight = true;
        } else if (c == '-' || c == '<' || c == '>' || c == '.' ||
                   std::isdigit(static_cast<unsigned char>(c))) {
            opChars += c;
            ++pos;
        } else {
            break;
        }
    }

    if (opChars.empty() && !hasQuotedWeight) {
        error = "Empty operator";
        return {std::string::npos, false, 1.0, false, false};
    }

    // ── 裁剪操作符末尾的右顶点数字 ──
    size_t effectiveEnd = opChars.size();
    for (size_t i = opChars.size(); i > 0; --i) {
        char c = opChars[i - 1];
        if (c == '>' || c == '-') {
            effectiveEnd = i;
            break;
        }
    }
    if (effectiveEnd < opChars.size()) {
        pos -= (opChars.size() - effectiveEnd);
        opChars.resize(effectiveEnd);
    }

    // ── 结构合法性校验 ──
    if (!opChars.empty()) {
        // 操作符必须以 - 或 < 开头（已由 first 保证）
        // 操作符必须以 - 或 > 结尾
        char last = opChars.back();
        if (last != '-' && last != '>') {
            error = "Operator must end with '-' or '>'";
            return {std::string::npos, false, 1.0, false, false};
        }

        // < 只能出现在开头
        for (size_t i = 1; i < opChars.size(); ++i) {
            if (opChars[i] == '<') {
                error = "Invalid operator: '<' must be at the beginning";
                return {std::string::npos, false, 1.0, false, false};
            }
        }

        // > 只能出现在末尾
        for (size_t i = 0; i + 1 < opChars.size(); ++i) {
            if (opChars[i] == '>') {
                error = "Invalid operator: '>' must be at the end";
                return {std::string::npos, false, 1.0, false, false};
            }
        }
    }

    // ── 分析操作符 ──
    bool directed = false;
    bool hasBidi = false;
    double weight = 1.0;
    bool explicit_weight = false;

    for (char c : opChars) {
        if (c == '>') directed = true;
        if (c == '<') hasBidi = true;
    }

    // 方向分析
    bool reverse = false;
    if (directed && hasBidi) {
        // 同时含 < 和 > → 无向
        directed = false;
    } else if (hasBidi && !directed) {
        // 仅含 < 不含 > → 有向（右→左）
        directed = true;
        reverse = true;
    }

    // 提取权重
    if (hasQuotedWeight) {
        try {
            weight = std::stod(quotedWeightStr);
        } catch (...) { weight = 1.0; }
        explicit_weight = true;
    } else {
        std::string numStr;
        bool inNum = false;
        for (size_t i = 0; i < opChars.size(); ++i) {
            char c = opChars[i];
            if (std::isdigit(static_cast<unsigned char>(c)) || c == '.') {
                if (!inNum) { inNum = true; numStr.clear(); }
                numStr += c;
            } else {
                if (inNum) break;
            }
        }
        if (!numStr.empty()) {
            try {
                weight = std::stod(numStr);
            } catch (...) { weight = 1.0; }
            explicit_weight = true;
        }
    }

    return {pos, directed, weight, explicit_weight, reverse};
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
    std::string opError;
    auto [opEnd, directed, weight, explicit_w, reverse] = parseOperator(line, pos, opError);
    if (opEnd == std::string::npos) {
        error = opError.empty()
            ? "Cannot parse edge operator after \"" + left + "\""
            : opError;
        return false;
    }

    // 3. 解析右顶点
    auto [right, _] = parseVertexName(line, opEnd);
    if (right.empty()) {
        error = "Missing right vertex name after operator";
        return false;
    }

    // 4. 添加边（reverse 时交换方向：a<---b → b→a）
    if (reverse)
        graph.addEdge(right, left, weight, directed, explicit_w);
    else
        graph.addEdge(left, right, weight, directed, explicit_w);
    return true;
}

std::string GraphParser::serialize(const Graph& graph)
{
    std::ostringstream oss;
    auto edges = graph.getAllEdges();
    for (const auto& e : edges) {
        std::string fromStr = needsQuoting(e.from) ? quoteName(e.from) : e.from;
        std::string toStr   = needsQuoting(e.to)   ? quoteName(e.to)   : e.to;

        // 格式化权重字符串
        std::string wStr;
        bool showWeight = e.explicit_weight;

        if (showWeight) {
            if (e.weight < 0)
                wStr = "\"" + std::to_string(static_cast<int>(e.weight)) + "\"";  // 负权用引号
            else if (e.weight == static_cast<int>(e.weight))
                wStr = std::to_string(static_cast<int>(e.weight));
            else
                wStr = std::to_string(e.weight);
            // 去掉末尾多余的 0
            if (wStr.find('.') != std::string::npos) {
                while (wStr.back() == '0') wStr.pop_back();
                if (wStr.back() == '.') wStr.pop_back();
            }
        }

        if (e.directed) {
            if (!showWeight || e.weight == 1.0)
                oss << fromStr << "-->" << toStr << "\n";
            else
                oss << fromStr << "-" << wStr << "->" << toStr << "\n";
        } else {
            if (!showWeight || e.weight == 1.0)
                oss << fromStr << "---" << toStr << "\n";
            else
                oss << fromStr << "-" << wStr << "--" << toStr << "\n";
        }
    }
    return oss.str();
}
