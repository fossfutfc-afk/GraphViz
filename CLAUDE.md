# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build & Run

```bash
# Configure & build (GUI with Qt6 MinGW)
cmake --preset gui -DCMAKE_BUILD_TYPE=Release
cmake --build build-gui --config Release

# Run
./build-gui/GraphViz.exe

# CLI-only build (no Qt dependency)
cmake --preset default
cmake --build build
```

**Prerequisites**: CMake ≥ 3.19, C++17 compiler (MinGW), Qt 6.11.1 (Widgets). The preset hardcodes Qt paths in `CMakePresets.json` — update `CMAKE_PREFIX_PATH`, `CMAKE_C_COMPILER`, `CMAKE_CXX_COMPILER` for other machines. Post-build, `windeployqt` auto-deploys Qt DLLs into the build directory.

No test suite exists (`enable_testing()` is in CMakeLists.txt but tests were removed in commit `7655059`). Manual verification: launch the app and load files from `test_data/` or `samples/`.

**`.gitignore` notes**: `dist/` and `test_data/` are git-ignored for new files (existing tracked files in `test_data/` remain tracked). `samples/` is fully tracked — place distributable examples there. Build artifacts (`build*/`, `*.exe`, `*.o`) are ignored.

## Architecture

**Two-layer design**: Core library (no Qt) + GUI layer (Qt6 Widgets).

### Core layer (`include/` + `src/` without `src/gui/`)

| File | Role |
|------|------|
| `GraphTypes.h` | POD structs: `Vertex` (name, display_name, id), `Edge` (from/to/weight/directed/id/explicit_weight) |
| `Graph` | Adjacency list storage (`unordered_map<string, vector<Edge>>`). Undirected edges stored bidirectionally with shared `id`. Supports parallel edges, self-loops, mixed directed/undirected. `hasExplicitWeight()` gates global weight display. |
| `GraphParser` | Static `parse()` — manual char-scanning parser (not regex). Handles quoted vertex names with escapes, flexible arrow syntax (`-->`, `<--`, `<-->`, `---`), same-name nodes via `name(N)` suffix. Returns `Graph` + error list. |
| `GraphAlgorithm` | All static methods returning result structs: Dijkstra, Kruskal, Tarjan (articulation/bridges), Hierholzer (Euler), backtracking Hamilton (≤20 vertices), BFS components, Kosaraju SCC, planarity check. |

### GUI layer (`src/gui/`)

| File | Role |
|------|------|
| `MainWindow` | Top-level window: menu bar, left editor panel, right canvas, bottom control bar. Owns `Graph*`, orchestrates parse→render→algorithm→highlight flow. |
| `GraphWidget` | QPainter-based rendering: node circles, directed arrows, self-loop arcs, parallel edge offsets (±7px), weight labels. Mouse drag for vertex repositioning. Highlight via `setPathHighlight`/`setEdgeHighlight`/`setComponentHighlight`. |
| `GraphTextEdit` | QPlainTextEdit subclass adding Shift+Tab (Key_Backtab) for forward de-indent. |
| `ForceLayout` | Fruchterman-Reingold: ring initial placement, multiplicative cooling (0.969^iter × 150 iters), soft boundary, deterministic (seed 42). |

### Data flow

```
Text input → GraphParser::parse() → Graph (adjacency list)
                                       ↓
                              ForceLayout computes positions
                                       ↓
                              GraphWidget::setGraph() renders
                                       ↓
                     User selects algorithm → GraphAlgorithm::staticMethod()
                                       ↓
                     Result struct → MainWindow applies highlight to GraphWidget
```

### Key design decisions

- **No graph mutation during algorithm execution** — algorithms take `const Graph&` and return result structs; highlighting is separate.
- **Edge identity**: `Edge::id` is globally unique. Undirected reverse edges share the same id. `getAllEdges()` deduplicates by id.
- **Vertex identity**: `name` is the internal key (e.g., `"A"` or `"A#1"` for same-name nodes). `display_name` is what the user sees (e.g., `"A"`). `resolveVertexName()` maps user input to internal keys.
- **Same-name nodes** (v1.1.0): `2(1)---5` creates internal name `2#1` with display name `2`. Quoted names don't trigger suffix parsing.
- **Isolated vertices** (v1.2.0): A line containing only a vertex name (unquoted: no spaces/`-`; or quoted: `"..."` with escapes) creates a standalone vertex with no edges. `serialize()` outputs them after all edges. Most algorithms already handle isolated vertices correctly; planarity and Hamilton thresholds use `nNonIsolated` (vertices with degree > 0) to avoid false negatives.
- **All algorithms are static** — no state, no inheritance. Each returns a dedicated result struct (`PathResult`, `EulerResult`, `HamiltonResult`, etc.).
- **Hamilton/Euler multi-solution**: Small graphs collect all solutions (up to 100); UI provides prev/next navigation. Large graphs fall back to greedy single solution.

## Release Workflow

### Git 管理

- **约定式提交**：`feat:` / `fix:` / `docs:` / `refactor:` / `chore:`，commit message 使用英文。
- 任务完成时自动提交并推送（如有 remote）。推送失败仅提示一次。
- 自动提交产生冲突时自动取消并提示用户手动解决。

### 代码修改后 — 构建可执行程序包

每次完成代码修改并构建成功后，在 `dist/` 下生成便携版程序包：

```bash
# 1. 更新 CMakeLists.txt 中的 VERSION（如 project(GraphViz VERSION 1.2.0 ...)）
#    版本号需与即将发布的 tag 一致，然后提交此变更

# 2. 构建 Release 版本（确保使用最新代码）
cmake --preset gui -DCMAKE_BUILD_TYPE=Release
cmake --build build-gui --config Release

# 3. 创建便携包目录并复制文件
VERSION="1.2.0"  # 与 CMakeLists.txt 中一致
DIST_DIR="dist/GraphViz-v${VERSION}-portable"
mkdir -p "${DIST_DIR}"

# 复制可执行文件 & Qt DLL（windeployqt 已部署在 build-gui/ 下）
cp build-gui/GraphViz.exe "${DIST_DIR}/"
cp build-gui/*.dll "${DIST_DIR}/"

# 复制 Qt 插件目录
cp -r build-gui/platforms "${DIST_DIR}/"
cp -r build-gui/styles "${DIST_DIR}/"
cp -r build-gui/imageformats "${DIST_DIR}/"
cp -r build-gui/iconengines "${DIST_DIR}/"
cp -r build-gui/networkinformation "${DIST_DIR}/"
cp -r build-gui/tls "${DIST_DIR}/"
cp -r build-gui/generic "${DIST_DIR}/"

# 复制示例文件
cp -r samples "${DIST_DIR}/"

# 4. 打包 zip
7z a "dist/GraphViz-v${VERSION}-portable.zip" "./${DIST_DIR}/*"
```

**注意**：`dist/` 已在 `.gitignore` 中，本地包不入 Git。

### 用户验证认可后 — 发布 GitHub Release

**⚠️ 此步骤仅在用户明确确认后执行。**

```bash
VERSION="1.1.1"
git tag "v${VERSION}"
git push origin main --tags
gh release create "v${VERSION}" \
    "dist/GraphViz-v${VERSION}-portable.zip" \
    --title "GraphViz v${VERSION}" \
    --generate-notes
```

### 版本号规则

沿用已有 `v<major>.<minor>.<patch>` 格式（参考已有标签 v1.0.1 ~ v1.1.0）：
- **major**：重大架构变更或不兼容的 UI 改动
- **minor**：新功能（新算法、新输入语法等）
- **patch**：Bug 修复、性能优化、文档更新
