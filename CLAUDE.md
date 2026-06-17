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

**Prerequisites**: CMake ≥ 3.19, C++17 compiler (MinGW), Qt 6 (Widgets module). The preset hardcodes Qt paths in `CMakePresets.json` — update `CMAKE_PREFIX_PATH`, `CMAKE_C_COMPILER`, `CMAKE_CXX_COMPILER` for other machines. Post-build, `windeployqt` auto-deploys Qt DLLs into the build directory.

No test suite exists (`enable_testing()` is in CMakeLists.txt but tests were removed). Manual verification: launch the app and load files from `test_data/` or `samples/`.

**`.gitignore` notes**: `dist/` and `test_data/` are git-ignored for new files (existing tracked files in `test_data/` remain tracked). `samples/` is fully tracked — place distributable example graphs there. Build artifacts (`build*/`, `*.exe`, `*.o`) are ignored.

## Architecture

**Two-layer design**: Core library (no Qt) + GUI layer (Qt6 Widgets).

### Core layer (`include/` + `src/` without `src/gui/`)

| File | Role |
|------|------|
| `GraphTypes.h` | POD structs: `Vertex` (name, display_name, id), `Edge` (from/to/weight/directed/id/explicit_weight) |
| `Graph` | Adjacency list storage (`unordered_map<string, vector<Edge>>`). Undirected edges stored bidirectionally with shared `id`. Supports parallel edges, self-loops, mixed directed/undirected. `hasExplicitWeight()` gates global weight display. |
| `GraphParser` | Static `parse()` — manual char-scanning parser (not regex). Handles quoted vertex names with escapes, flexible arrow syntax (`-->`, `<--`, `<-->`, `---`), same-name nodes via `name(N)` suffix, and isolated vertices (bare name on a line). Returns `Graph` + error list. `serialize()` converts back to text. |
| `GraphAlgorithm` | All static methods returning result structs: Dijkstra, Kruskal, Tarjan (articulation/bridges), Hierholzer (Euler), backtracking Hamilton, BFS components, Kosaraju SCC, planarity check. |

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
- **Same-name nodes**: `2(1)---5` creates internal name `2#1` with display name `2`. Quoted names don't trigger suffix parsing.
- **Isolated vertices**: A line containing only a vertex name (unquoted: no spaces or `-`; or quoted: `"..."` with escapes) creates a standalone vertex with no edges. `serialize()` outputs them after all edges. Planarity and Hamilton thresholds use `nNonIsolated` (vertices with degree > 0) to avoid false negatives from inflated vertex counts.
- **All algorithms are static** — no state, no inheritance. Each returns a dedicated result struct (`PathResult`, `EulerResult`, `HamiltonResult`, `PlanarityResult`, etc.).
- **Multi-solution support**: Euler and Hamilton algorithms collect all solutions (up to 100) on small graphs; UI provides prev/next navigation. Large graphs fall back to greedy single solution.

## Releasing

Typical workflow: plan → implement → update README → build → package → user tests → user approves → tag → write release notes → publish.

When the user asks to create a release, follow these steps. Do not release without explicit confirmation.

### User testing (before packaging)

Before creating the zip and publishing, let the user test with an unzipped portable directory:

1. Ask which version number to release.
2. Bump `CMakeLists.txt` `project()` VERSION, then build.
3. Assemble the portable directory under `dist/GraphViz-v<version>-portable/` (do NOT zip yet).
4. The user launches `GraphViz.exe` from **inside that portable directory** — not from `build-gui/`. This ensures all bundled files (MANUAL.html, samples, Qt plugins) are found correctly at their intended paths relative to the executable.
5. After the user confirms tests pass, create the zip and proceed to tag + release.

### Build a portable package

```bash
# 1. Ensure CMakeLists.txt project() VERSION is set to the desired release version.
#    Commit the version bump before building.

# 2. Build Release
cmake --preset gui -DCMAKE_BUILD_TYPE=Release
cmake --build build-gui --config Release

# 3. Assemble portable directory
VERSION="<version>"   # must match CMakeLists.txt
DIST_DIR="dist/GraphViz-v${VERSION}-portable"
mkdir -p "${DIST_DIR}"

cp build-gui/GraphViz.exe "${DIST_DIR}/"
cp build-gui/*.dll "${DIST_DIR}/"
cp -r build-gui/platforms "${DIST_DIR}/"
cp -r build-gui/styles "${DIST_DIR}/"
cp -r build-gui/imageformats "${DIST_DIR}/"
cp -r build-gui/iconengines "${DIST_DIR}/"
cp -r build-gui/networkinformation "${DIST_DIR}/"
cp -r build-gui/tls "${DIST_DIR}/"
cp -r build-gui/generic "${DIST_DIR}/"
cp docs/MANUAL.html "${DIST_DIR}/"
cp -r samples "${DIST_DIR}/"

# 4. Create zip
7z a "dist/GraphViz-v${VERSION}-portable.zip" "./${DIST_DIR}/*"
```

`dist/` is in `.gitignore`; packages stay local and are not committed.

### Publish a GitHub Release (user confirmation required)

**Write release notes.** Check the previous release for format and style consistency:
```bash
gh release view <previous-tag> --json body
```
Write notes covering new features, fixes, and changed files. Match the existing
language and section structure (heading, changelog, test plan, file list, etc.).

**Tag, push, and create the release:**
```bash
VERSION="<version>"
git tag "v${VERSION}"
git push origin main --tags
gh release create "v${VERSION}" \
    "dist/GraphViz-v${VERSION}-portable.zip" \
    --title "GraphViz v${VERSION}" \
    --notes "<release notes>"
```

If the notes need revision after publishing, use `gh release edit v${VERSION} --notes "..."`.
