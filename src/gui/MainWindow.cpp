#include "MainWindow.h"
#include "GraphWidget.h"
#include "GraphTextEdit.h"
#include "UpdateChecker.h"
#include "Graph.h"
#include "GraphParser.h"
#include "GraphAlgorithm.h"

#include <QApplication>
#include <QComboBox>
#include <QDesktopServices>
#include <QFile>
#include <QFileDialog>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QSplitter>
#include <QStatusBar>
#include <QTextBlock>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include <fstream>
#include <sstream>

// 示例图数据
static const char* SAMPLE_GRAPH =
    "# GraphViz 示例 — 基础用法\n"
    "# 详细说明请参阅 帮助 → 使用说明\n"
    "#\n"
    "# 无向边 (---):\n"
    "A---B\n"
    "B---C\n"
    "A---C\n"
    "# 有向边 (-->):\n"
    "C-->D\n";

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_graph(new Graph())
{
    setWindowTitle(QString("GraphViz v%1 — 图可视化工具").arg(GRAPHVIZ_VERSION));
    resize(1200, 750);

    // ── 菜单栏 ──
    QMenu *fileMenu = menuBar()->addMenu("文件(&F)");
    fileMenu->addAction("打开(&O)...", QKeySequence::Open, this, &MainWindow::onOpenFile);
    fileMenu->addAction("保存(&S)...", QKeySequence::Save, this, &MainWindow::onSaveFile);
    fileMenu->addSeparator();
    fileMenu->addAction("退出(&Q)", QKeySequence::Quit, this, &QWidget::close);

    QMenu *editMenu = menuBar()->addMenu("编辑(&E)");
    editMenu->addAction("撤销(&U)", QKeySequence::Undo, this, &MainWindow::onUndo);
    editMenu->addAction("重做(&R)", QKeySequence::Redo, this, &MainWindow::onRedo);

    QMenu *viewMenu = menuBar()->addMenu("视图(&V)");
    viewMenu->addAction("重置布局(&R)", this, &MainWindow::onResetLayout);

    // ── 帮助菜单 ──
    QMenu *helpMenu = menuBar()->addMenu("帮助(&H)");
    helpMenu->addAction("检查更新(&U)", this, &MainWindow::onCheckForUpdates);
    helpMenu->addAction("打开下载页(&D)", this, &MainWindow::onOpenDownloadPage);
    helpMenu->addAction("使用说明(&H)", this, &MainWindow::onOpenManual);
    helpMenu->addSeparator();
    helpMenu->addAction("关于(&A)", this, &MainWindow::onAbout);

    // ── 中央区域：水平分割 ──
    QSplitter *splitter = new QSplitter(Qt::Horizontal, this);
    splitter->setChildrenCollapsible(false);

    // 左侧面板
    splitter->addWidget(createInputPanel());

    // 右侧图可视化
    m_graphWidget = new GraphWidget(this);
    splitter->addWidget(m_graphWidget);

    splitter->setStretchFactor(0, 1);   // 左侧 1
    splitter->setStretchFactor(1, 3);   // 右侧 3

    // ── 整体布局 ──
    QWidget *central = new QWidget(this);
    QVBoxLayout *mainLayout = new QVBoxLayout(central);
    mainLayout->addWidget(splitter, 1);
    mainLayout->addWidget(createControlBar(), 0);

    setCentralWidget(central);

    // ── 状态栏 ──
    m_statusLabel = new QLabel("就绪");
    statusBar()->addWidget(m_statusLabel);

    // ── 更新检查器 ──
    m_updateChecker = new UpdateChecker(this);
    connect(m_updateChecker, &UpdateChecker::updateCheckFinished,
            this, [this](UpdateChecker::UpdateStatus status, const QString &message) {
        switch (status) {
        case UpdateChecker::UpdateStatus::UpdateAvailable:
            // Persistent — stays until user acts
            updateStatus(QString("发现新版本 %1 — 点击\"帮助→打开下载页\"下载")
                             .arg(message));
            break;
        case UpdateChecker::UpdateStatus::AlreadyLatest:
            updateStatus("已是最新版本");
            QTimer::singleShot(5000, this, [this]() {
                if (m_statusLabel->text() == "已是最新版本")
                    updateStatus("就绪");
            });
            break;
        case UpdateChecker::UpdateStatus::Error:
            updateStatus(QString("检查更新失败: %1").arg(message));
            QTimer::singleShot(5000, this, [this]() {
                if (m_statusLabel->text().startsWith("检查更新失败"))
                    updateStatus("就绪");
            });
            break;
        }
    });
}

MainWindow::~MainWindow()
{
    delete m_graph;
}

void MainWindow::showEvent(QShowEvent *event)
{
    QMainWindow::showEvent(event);
    // 首次显示时加载示例图
    static bool firstShow = true;
    if (firstShow) {
        firstShow = false;
        loadSampleGraph();

        // 启动后 ~2s 异步检查更新
        QTimer::singleShot(2000, this, &MainWindow::onCheckForUpdates);
    }
}

// ── 输入面板 ──
QWidget* MainWindow::createInputPanel()
{
    QWidget *panel = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(panel);

    QLabel *label = new QLabel("图数据输入（每行一条边）:");
    layout->addWidget(label);

    m_textEdit = new GraphTextEdit(this);
    m_textEdit->setFont(QFont("Consolas", 10));
    m_textEdit->setPlaceholderText(
        "输入边数据，每行一条：\n"
        "  a-->b    有向无权边\n"
        "  a-2.5->b 有向带权边\n"
        "  a---b    无向无权边\n"
        "  a-3--b   无向带权边\n"
        "支持 # 注释和空行");
    layout->addWidget(m_textEdit, 1);

    return panel;
}

// ── 控制栏 ──
QWidget* MainWindow::createControlBar()
{
    QWidget *bar = new QWidget(this);
    QHBoxLayout *layout = new QHBoxLayout(bar);

    // 文件操作
    QPushButton *btnOpen = new QPushButton("加载文件", this);
    connect(btnOpen, &QPushButton::clicked, this, &MainWindow::onOpenFile);
    layout->addWidget(btnOpen);

    QPushButton *btnParse = new QPushButton("解析并渲染", this);
    btnParse->setStyleSheet("font-weight: bold;");
    connect(btnParse, &QPushButton::clicked, this, &MainWindow::onParseAndRender);
    layout->addWidget(btnParse);

    QPushButton *btnUndo = new QPushButton("↩ 撤销", this);
    btnUndo->setToolTip("撤销文本编辑 (Ctrl+Z)");
    connect(btnUndo, &QPushButton::clicked, this, &MainWindow::onUndo);
    layout->addWidget(btnUndo);

    QPushButton *btnRedo = new QPushButton("↪ 重做", this);
    btnRedo->setToolTip("重做文本编辑 (Ctrl+Y)");
    connect(btnRedo, &QPushButton::clicked, this, &MainWindow::onRedo);
    layout->addWidget(btnRedo);

    layout->addSpacing(20);

    // 算法选择
    QLabel *algoLabel = new QLabel("算法:", this);
    layout->addWidget(algoLabel);

    m_algoCombo = new QComboBox(this);
    m_algoCombo->addItems({
        "最短路径 (Dijkstra)",
        "最小生成树 (Kruskal)",
        "关节点 & 桥 (Tarjan)",
        "欧拉回路 (Hierholzer)",
        "欧拉通路",
        "哈密顿回路 (回溯)",
        "哈密顿通路 (回溯)",
        "连通分量 (BFS)",
        "强连通分量 (Kosaraju)",
        "平面性检测 (Planarity)",
    });
    layout->addWidget(m_algoCombo);

    // 参数输入
    QLabel *fromLabel = new QLabel("起点:", this);
    layout->addWidget(fromLabel);
    m_fromEdit = new QLineEdit(this);
    m_fromEdit->setPlaceholderText("起点");
    m_fromEdit->setMaximumWidth(80);
    layout->addWidget(m_fromEdit);

    QLabel *toLabel = new QLabel("终点:", this);
    layout->addWidget(toLabel);
    m_toEdit = new QLineEdit(this);
    m_toEdit->setPlaceholderText("终点");
    m_toEdit->setMaximumWidth(80);
    layout->addWidget(m_toEdit);

    QPushButton *btnExec = new QPushButton("执行", this);
    btnExec->setStyleSheet("font-weight: bold; color: #0060c0;");
    connect(btnExec, &QPushButton::clicked, this, &MainWindow::onExecuteAlgorithm);
    layout->addWidget(btnExec);

    layout->addSpacing(15);

    QPushButton *btnClearHL = new QPushButton("清除高亮", this);
    connect(btnClearHL, &QPushButton::clicked, this, &MainWindow::onClearHighlights);
    layout->addWidget(btnClearHL);

    QPushButton *btnClearAll = new QPushButton("清除全部", this);
    connect(btnClearAll, &QPushButton::clicked, this, &MainWindow::onClearAll);
    layout->addWidget(btnClearAll);

    layout->addSpacing(8);

    m_btnPrevSolution = new QPushButton("< 上一解", this);
    m_btnPrevSolution->setEnabled(false);
    m_btnPrevSolution->setToolTip("查看上一个解");
    connect(m_btnPrevSolution, &QPushButton::clicked, this, &MainWindow::onPrevSolution);
    layout->addWidget(m_btnPrevSolution);

    m_btnNextSolution = new QPushButton("下一解 >", this);
    m_btnNextSolution->setEnabled(false);
    m_btnNextSolution->setToolTip("查看下一个解");
    connect(m_btnNextSolution, &QPushButton::clicked, this, &MainWindow::onNextSolution);
    layout->addWidget(m_btnNextSolution);

    layout->addStretch();

    return bar;
}

// ── 加载示例图 ──
void MainWindow::loadSampleGraph()
{
    m_textEdit->setPlainText(QString::fromUtf8(SAMPLE_GRAPH));
    onParseAndRender();
}

// ── 文件操作 ──
void MainWindow::onOpenFile()
{
    QString path = QFileDialog::getOpenFileName(
        this, "打开图数据文件", QString(),
        "图数据文件 (*.graph *.txt);;所有文件 (*.*)");
    if (path.isEmpty()) return;

    std::ifstream file(path.toStdString());
    if (!file) {
        QMessageBox::warning(this, "错误", "无法打开文件: " + path);
        return;
    }
    std::ostringstream oss;
    oss << file.rdbuf();
    m_textEdit->setPlainText(QString::fromStdString(oss.str()));
    onParseAndRender();
    updateStatus("已加载: " + path);
}

void MainWindow::onSaveFile()
{
    QString path = QFileDialog::getSaveFileName(
        this, "保存图数据文件", QString(),
        "图数据文件 (*.graph);;所有文件 (*.*)");
    if (path.isEmpty()) return;

    std::string data = GraphParser::serialize(*m_graph);
    std::ofstream file(path.toStdString());
    if (!file) {
        QMessageBox::warning(this, "错误", "无法写入文件: " + path);
        return;
    }
    file << data;
    updateStatus("已保存: " + path);
}

// ── 解析并渲染 ──
void MainWindow::onParseAndRender()
{
    std::string text = m_textEdit->toPlainText().toStdString();
    std::vector<std::string> errors;

    delete m_graph;
    m_graph = new Graph();
    *m_graph = GraphParser::parse(text, errors);

    if (!errors.empty()) {
        QString msg = "解析警告:\n";
        int count = 0;
        for (const auto& e : errors) {
            if (++count > 5) { msg += "... 等更多错误\n"; break; }
            msg += QString::fromStdString(e) + "\n";
        }
        updateStatus(msg);
    } else {
        updateStatus(QString("解析完成: %1 个顶点, %2 条边")
            .arg(m_graph->vertexCount())
            .arg(m_graph->edgeCount()));
    }

    m_graphWidget->setGraph(m_graph);
}

// ── 执行算法 ──
void MainWindow::onExecuteAlgorithm()
{
    if (!m_graph || m_graph->vertexCount() == 0) {
        QMessageBox::information(this, "提示", "请先输入图数据并解析。");
        return;
    }

    int algoIndex = m_algoCombo->currentIndex();
    QString from = m_fromEdit->text().trimmed();
    QString to = m_toEdit->text().trimmed();

    m_graphWidget->clearHighlights();
    clearSolutionButtons();

    // 辅助: 将顶点序列转为 display_name 字符串
    auto displaySeq = [&](const std::vector<std::string>& nodes) -> QString {
        QStringList parts;
        for (const auto& n : nodes)
            parts.append(QString::fromStdString(m_graph->getDisplayName(n)));
        return parts.join(" → ");
    };

    try {
        switch (algoIndex) {
        case 0: { // 最短路径
            if (from.isEmpty() || to.isEmpty()) {
                QMessageBox::information(this, "提示", "请输入起点和终点。");
                return;
            }
            std::string fromKey = m_graph->resolveVertexName(from.toStdString());
            std::string toKey   = m_graph->resolveVertexName(to.toStdString());
            auto result = GraphAlgorithm::shortestPath(
                *m_graph, fromKey, toKey);
            if (result.found) {
                QVector<QString> nodes;
                for (const auto& n : result.nodes)
                    nodes.append(QString::fromStdString(n));
                m_graphWidget->setPathHighlight(nodes);
                updateStatus(QString("最短路径: 权重 %1, %2")
                    .arg(result.totalWeight).arg(displaySeq(result.nodes)));
            } else {
                updateStatus("未找到最短路径（不连通？）");
            }
            break;
        }
        case 1: { // MST
            auto result = GraphAlgorithm::minimumSpanningTree(*m_graph);
            if (result.connected) {
                QVector<QString> edges;
                for (const auto& e : result.edges)
                    edges.append(QString::fromStdString(e));
                m_graphWidget->setEdgeHighlight(edges, HighlightType::MST);
                updateStatus(QString("最小生成树: 总权重 %1, %2 条边")
                    .arg(result.totalWeight).arg(result.edges.size()));
            } else {
                updateStatus("图不连通，最小生成树不存在");
            }
            break;
        }
        case 2: { // 关节点 & 桥
            auto result = GraphAlgorithm::findCritical(*m_graph);
            QVector<QString> nodes;
            QVector<QPair<QString, QString>> edges;
            for (const auto& n : result.articulationPoints)
                nodes.append(QString::fromStdString(n));
            for (const auto& e : result.bridges) {
                edges.append({QString::fromStdString(e.first),
                              QString::fromStdString(e.second)});
            }
            m_graphWidget->setCriticalHighlight(nodes, edges);
            updateStatus(QString("关节点: %1 个, 桥: %2 条")
                .arg(nodes.size()).arg(edges.size()));
            break;
        }
        case 3: { // 欧拉回路
            std::string startV;
            if (!from.isEmpty())
                startV = m_graph->resolveVertexName(from.toStdString());
            auto result = GraphAlgorithm::eulerCircuit(*m_graph, startV);
            m_lastEulerResult = result;
            m_hasEulerResult = result.exists && result.allSolutions.size() > 1;
            if (result.exists) {
                applyEulerHighlight(result.nodes, result.isCircuit);
                if (result.allSolutions.size() > 1) {
                    updateStatus(QString("欧拉回路: 解 1/%1, %2")
                        .arg(result.allSolutions.size()).arg(displaySeq(result.nodes)));
                    m_btnPrevSolution->setEnabled(true);
                    m_btnNextSolution->setEnabled(true);
                } else {
                    updateStatus("欧拉回路: " + displaySeq(result.nodes));
                }
            } else {
                updateStatus(QString::fromStdString(
                    "不存在欧拉回路: " + result.message));
            }
            break;
        }
        case 4: { // 欧拉通路
            std::string startV;
            if (!from.isEmpty())
                startV = m_graph->resolveVertexName(from.toStdString());
            auto result = GraphAlgorithm::eulerTrail(*m_graph, startV);
            m_lastEulerResult = result;
            m_hasEulerResult = result.exists && result.allSolutions.size() > 1;
            if (result.exists) {
                applyEulerHighlight(result.nodes, result.isCircuit);
                QString type = result.isCircuit ? "回路" : "通路";
                if (result.allSolutions.size() > 1) {
                    updateStatus(QString("欧拉%1: 解 1/%2, %3")
                        .arg(type).arg(result.allSolutions.size())
                        .arg(displaySeq(result.nodes)));
                    m_btnPrevSolution->setEnabled(true);
                    m_btnNextSolution->setEnabled(true);
                } else {
                    updateStatus("欧拉" + type + ": " + displaySeq(result.nodes));
                }
            } else {
                updateStatus(QString::fromStdString(
                    "不存在欧拉通路: " + result.message));
            }
            break;
        }
        case 5: { // 哈密顿回路
            std::string startV;
            if (!from.isEmpty())
                startV = m_graph->resolveVertexName(from.toStdString());
            auto result = GraphAlgorithm::hamiltonCircuit(*m_graph, startV);
            m_lastHamiltonResult = result;
            m_hasHamiltonResult = result.found && result.allSolutions.size() > 1;
            if (result.found) {
                applyHamiltonHighlight(result.nodes, result.isCircuit);
                if (result.allSolutions.size() > 1) {
                    updateStatus(QString("哈密顿回路: 解 1/%1, %2 个顶点")
                        .arg(result.allSolutions.size()).arg(result.nodes.size() - 1));
                    m_btnPrevSolution->setEnabled(true);
                    m_btnNextSolution->setEnabled(true);
                } else {
                    updateStatus(QString("哈密顿回路: %1 个顶点 (唯一解)")
                        .arg(result.nodes.size() - 1));
                }
            } else {
                updateStatus("未找到哈密顿回路: " +
                            QString::fromStdString(result.message));
            }
            break;
        }
        case 6: { // 哈密顿通路
            std::string startV;
            if (!from.isEmpty())
                startV = m_graph->resolveVertexName(from.toStdString());
            auto result = GraphAlgorithm::hamiltonPath(*m_graph, startV);
            m_lastHamiltonResult = result;
            m_hasHamiltonResult = result.found && result.allSolutions.size() > 1;
            if (result.found) {
                applyHamiltonHighlight(result.nodes, result.isCircuit);
                if (result.allSolutions.size() > 1) {
                    updateStatus(QString("哈密顿通路: 解 1/%1, %2 个顶点")
                        .arg(result.allSolutions.size()).arg(result.nodes.size()));
                    m_btnPrevSolution->setEnabled(true);
                    m_btnNextSolution->setEnabled(true);
                } else {
                    updateStatus(QString("哈密顿通路: %1 个顶点 (唯一解)")
                        .arg(result.nodes.size()));
                }
            } else {
                updateStatus("未找到哈密顿通路: " +
                            QString::fromStdString(result.message));
            }
            break;
        }
        case 7: { // 连通分量
            auto result = GraphAlgorithm::connectedComponents(*m_graph);
            QVector<QVector<QString>> comps;
            for (const auto& comp : result.components) {
                QVector<QString> qcomp;
                for (const auto& n : comp)
                    qcomp.append(QString::fromStdString(n));
                comps.append(qcomp);
            }
            m_graphWidget->setComponentHighlight(comps);
            updateStatus(QString("连通分量: %1 个").arg(comps.size()));
            break;
        }
        case 8: { // 强连通分量
            auto result = GraphAlgorithm::stronglyConnectedComponents(*m_graph);
            QVector<QVector<QString>> comps;
            for (const auto& comp : result.components) {
                QVector<QString> qcomp;
                for (const auto& n : comp)
                    qcomp.append(QString::fromStdString(n));
                comps.append(qcomp);
            }
            m_graphWidget->setComponentHighlight(comps);
            updateStatus(QString("强连通分量: %1 个").arg(comps.size()));
            break;
        }
        case 9: { // 平面性检测
            auto result = GraphAlgorithm::checkPlanarity(*m_graph);
            if (result.isPlanar) {
                updateStatus(QString("✓ 平面图 — %1")
                    .arg(QString::fromStdString(result.message)));
            } else {
                updateStatus(QString("✗ 非平面图 — %1")
                    .arg(QString::fromStdString(result.message)));
            }
            break;
        }
        }
    } catch (const std::exception& e) {
        QMessageBox::warning(this, "算法错误",
                            QString("执行算法时发生异常:\n%1").arg(e.what()));
    }
}

void MainWindow::onClearHighlights()
{
    m_graphWidget->clearHighlights();
    updateStatus("高亮已清除");
}

void MainWindow::onClearAll()
{
    if (!m_textEdit->document()->isEmpty()) {
        auto ret = QMessageBox::question(
            this, "确认清除",
            "确定要清除全部内容吗？\n\n"
            "此操作将清空编辑区和图数据，不可撤销。",
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (ret != QMessageBox::Yes) return;
    }

    delete m_graph;
    m_graph = new Graph();
    m_graphWidget->setGraph(m_graph);
    m_textEdit->clear();
    updateStatus("已清空（可按 Ctrl+Z 撤销文本清除）");
}

void MainWindow::onUndo()
{
    m_textEdit->undo();
}

void MainWindow::onRedo()
{
    m_textEdit->redo();
}

void MainWindow::onResetLayout()
{
    if (m_graph && m_graph->vertexCount() > 0) {
        m_graphWidget->computeLayout();
        m_graphWidget->update();
        updateStatus("布局已重置");
    }
}

void MainWindow::updateStatus(const QString& msg)
{
    m_statusLabel->setText(msg);
}

void MainWindow::applyHamiltonHighlight(const std::vector<std::string>& nodes, bool isCircuit)
{
    QVector<QString> edges;
    size_t end = isCircuit ? nodes.size() - 1 : nodes.size();
    for (size_t i = 0; i < end; ++i) {
        std::string key = nodes[i] + "|" + nodes[(i + 1) % nodes.size()];
        edges.append(QString::fromStdString(key));
    }
    m_graphWidget->setEdgeHighlight(edges, HighlightType::Hamilton);
}

void MainWindow::applyEulerHighlight(const std::vector<std::string>& nodes, bool isCircuit)
{
    QVector<QString> edges;
    size_t end = nodes.size() - 1;
    for (size_t i = 0; i < end; ++i) {
        std::string key = nodes[i] + "|" + nodes[i + 1];
        edges.append(QString::fromStdString(key));
    }
    m_graphWidget->setEdgeHighlight(edges, HighlightType::Euler);
}

void MainWindow::clearSolutionButtons()
{
    m_btnPrevSolution->setEnabled(false);
    m_btnNextSolution->setEnabled(false);
    m_hasHamiltonResult = false;
    m_hasEulerResult = false;
}

void MainWindow::onPrevSolution()
{
    auto displaySeq = [&](const std::vector<std::string>& nodes) -> QString {
        QStringList parts;
        for (const auto& n : nodes)
            parts.append(QString::fromStdString(m_graph->getDisplayName(n)));
        return parts.join(" → ");
    };
    if (m_hasEulerResult) {
        auto& result = m_lastEulerResult;
        int total = static_cast<int>(result.allSolutions.size());
        if (total <= 1) return;
        result.solutionIndex = (result.solutionIndex - 1 + total) % total;
        result.nodes = result.allSolutions[result.solutionIndex];
        applyEulerHighlight(result.nodes, result.isCircuit);
        updateStatus(QString("欧拉%1: 解 %2/%3, %4")
            .arg(result.isCircuit ? "回路" : "通路")
            .arg(result.solutionIndex + 1).arg(total)
            .arg(displaySeq(result.nodes)));
        return;
    }
    if (!m_hasHamiltonResult) return;
    auto& result = m_lastHamiltonResult;
    int total = static_cast<int>(result.allSolutions.size());
    if (total <= 1) return;

    result.solutionIndex = (result.solutionIndex - 1 + total) % total;
    result.nodes = result.allSolutions[result.solutionIndex];
    applyHamiltonHighlight(result.nodes, result.isCircuit);
    updateStatus(QString("哈密顿%1: 解 %2/%3, %4 个顶点")
        .arg(result.isCircuit ? "回路" : "通路")
        .arg(result.solutionIndex + 1).arg(total)
        .arg(result.nodes.size() - (result.isCircuit ? 1 : 0)));
}

void MainWindow::onNextSolution()
{
    auto displaySeq = [&](const std::vector<std::string>& nodes) -> QString {
        QStringList parts;
        for (const auto& n : nodes)
            parts.append(QString::fromStdString(m_graph->getDisplayName(n)));
        return parts.join(" → ");
    };
    if (m_hasEulerResult) {
        auto& result = m_lastEulerResult;
        int total = static_cast<int>(result.allSolutions.size());
        if (total <= 1) return;
        result.solutionIndex = (result.solutionIndex + 1) % total;
        result.nodes = result.allSolutions[result.solutionIndex];
        applyEulerHighlight(result.nodes, result.isCircuit);
        updateStatus(QString("欧拉%1: 解 %2/%3, %4")
            .arg(result.isCircuit ? "回路" : "通路")
            .arg(result.solutionIndex + 1).arg(total)
            .arg(displaySeq(result.nodes)));
        return;
    }
    if (!m_hasHamiltonResult) return;
    auto& result = m_lastHamiltonResult;
    int total = static_cast<int>(result.allSolutions.size());
    if (total <= 1) return;

    result.solutionIndex = (result.solutionIndex + 1) % total;
    result.nodes = result.allSolutions[result.solutionIndex];
    applyHamiltonHighlight(result.nodes, result.isCircuit);
    updateStatus(QString("哈密顿%1: 解 %2/%3, %4 个顶点")
        .arg(result.isCircuit ? "回路" : "通路")
        .arg(result.solutionIndex + 1).arg(total)
        .arg(result.nodes.size() - (result.isCircuit ? 1 : 0)));
}

// ── 帮助菜单 slots ──

void MainWindow::onCheckForUpdates()
{
    updateStatus("正在检查更新...");
    m_updateChecker->checkForUpdates();
}

void MainWindow::onOpenDownloadPage()
{
    QDesktopServices::openUrl(
        QUrl("https://github.com/SiriLee/GraphViz/releases"));
}

void MainWindow::onOpenManual()
{
    // Prefer the manual alongside the executable (portable package layout)
    QString manualPath = QApplication::applicationDirPath() + "/MANUAL.html";
    if (!QFile::exists(manualPath))
        manualPath = "docs/MANUAL.html";  // fallback when running from source tree
    QDesktopServices::openUrl(QUrl::fromLocalFile(manualPath));
}

void MainWindow::onAbout()
{
    QMessageBox::about(this, "关于 GraphViz",
        QString(
            "GraphViz v%1\n\n"
            "图论算法可视化工具\n"
            "支持有向/无向图的交互式编辑、自动布局与经典图论算法演示。\n\n"
            "Qt %2\n"
            "GitHub: https://github.com/SiriLee/GraphViz\n\n"
            "MIT License\n"
            "Copyright 2025 SiriLee")
        .arg(GRAPHVIZ_VERSION)
        .arg(QString::fromLatin1(qVersion())));
}
