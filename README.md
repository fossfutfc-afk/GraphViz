# GraphViz — 图可视化工具

基于 C++/Qt6 的图论可视化工具，支持有向图和无向图的手动构建、编辑、力导向布局展示，以及多种图论经典算法的高亮可视化。

## 功能概览

- **图输入**: 支持简洁文本格式 (`a-->b` / `a-2.5->b` / `a---b` / `a-3--b`)，每行一条边
- **图渲染**: 力导向布局 (Fruchterman-Reingold)，支持顶点拖拽调整
- **边类型**: 有向边（带箭头）、无向边、带权边（数值标签）
- **顶点**: 圆形节点 + 名称标签，支持鼠标拖拽

### 图论算法（9 种）

| 算法 | 方法 | 高亮颜色 |
|---|---|---|
| 最短路径 | Dijkstra | 绿色 |
| 最小生成树 | Kruskal | 橙色 |
| 关节点 & 桥 | Tarjan | 红色节点 / 紫色边 |
| 欧拉回路 | Hierholzer | 蓝色 |
| 欧拉通路 | Hierholzer 变体 | 蓝色 |
| 哈密顿回路 | 回溯 + 剪枝 | 青色 |
| 哈密顿通路 | 回溯 + 剪枝 | 青色 |
| 连通分量 | BFS | 多色 |
| 强连通分量 | Kosaraju | 多色 |

## 输入格式

```
# 无向边
A---B               # 无向无权边（权重默认 1.0）
B-3--E              # 无向带权边（权重 3）

# 有向边
G-->H               # 有向无权边（权重默认 1.0）
I-5->J              # 有向带权边（权重 5）

# 支持 # 开头的注释和空行
```

## 构建指南

### 依赖

- CMake ≥ 3.10
- GCC (MinGW-w64) 13.1.0+ 或其他支持 C++17 的编译器
- Qt 6.11.1+ (Widgets 模块)

### 构建步骤

```bash
# 克隆仓库
git clone https://github.com/SiriLee/GraphViz.git
cd GraphViz

# 配置（使用 Qt 6 MinGW 编译器）
cmake --preset gui

# 编译
cmake --build build-gui

# 运行
./build-gui/GraphViz.exe
```

> **注意**: 请根据本地 Qt 安装路径修改 `CMakePresets.json` 中的 `CMAKE_PREFIX_PATH`、`CMAKE_C_COMPILER`、`CMAKE_CXX_COMPILER`。

### CMakePresets.json 关键配置

```json
{
  "name": "gui",
  "cacheVariables": {
    "BUILD_GUI": "ON",
    "CMAKE_PREFIX_PATH": "<your_qt_path>/mingw_64",
    "CMAKE_C_COMPILER": "<your_mingw_path>/gcc.exe",
    "CMAKE_CXX_COMPILER": "<your_mingw_path>/g++.exe"
  }
}
```

## 使用说明

1. 启动程序后自动加载示例图
2. **左侧面板**: 编辑图数据（文本格式）
3. 点击 **「解析并渲染」** 构建图并显示
4. 在底部控制栏选择算法、输入参数（如起止顶点），点击 **「执行」**
5. 高亮结果显示在图中
6. 可用鼠标拖拽顶点调整位置
7. 通过菜单 **文件 → 保存** 导出图数据

### 操作按钮

| 按钮 | 功能 |
|---|---|
| 加载文件 | 打开 `.graph` / `.txt` 文件 |
| 解析并渲染 | 解析文本并刷新图显示 |
| 执行 | 运行选中的算法并高亮结果 |
| 清除高亮 | 移除高亮，保留图结构 |
| 清除全部 | 清空图数据和输入 |

## 项目结构

```
GraphViz/
├── CMakeLists.txt
├── CMakePresets.json
├── include/
│   ├── GraphTypes.h           # Vertex, Edge 数据结构
│   ├── Graph.h                # 图存储层（邻接表）
│   ├── GraphParser.h          # 文本格式解析器
│   └── GraphAlgorithm.h       # 算法接口 + 结果结构体
├── src/
│   ├── main.cpp               # 应用入口
│   ├── Graph.cpp              # 图存储实现
│   ├── GraphParser.cpp        # 解析器实现
│   ├── GraphAlgorithm.cpp     # 9 种图论算法实现
│   └── gui/
│       ├── MainWindow.h/cpp   # 主窗口（输入面板 + 控制栏）
│       ├── GraphWidget.h/cpp  # 图渲染组件（QPainter）
│       └── ForceLayout.h/cpp  # 力导向布局算法
└── test_data/
    └── sample.graph           # 示例数据
```

## 技术栈

- **语言**: C++17
- **构建**: CMake + MinGW Makefiles
- **GUI**: Qt 6.11.1 Widgets (QPainter 自绘)
- **布局**: Fruchterman-Reingold 力导向算法
- **无第三方依赖**（除 Qt 和 C++ 标准库）

## 设计说明

- 图数据模型同时支持有向边和无向边混合存储
- 边通过标准化键（无向: `min|max`，有向: `from|to`）去重
- 哈密顿回路/通路使用回溯 + 剪枝，顶点数限制 ≤ 20（超过自动跳过）
- 欧拉回路/通路使用 Hierholzer 算法，自动判断度数条件
- 力导向布局固定随机种子 (seed=42)，结果可复现

## License

MIT License — 仅供个人使用。
