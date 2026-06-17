#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <vector>
#include <string>

#include "GraphAlgorithm.h"

class Graph;
class GraphWidget;
class GraphTextEdit;
class UpdateChecker;
class QComboBox;
class QLineEdit;
class QLabel;
class QPushButton;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    void showEvent(QShowEvent *event) override;

private slots:
    void onOpenFile();
    void onSaveFile();
    void onParseAndRender();
    void onExecuteAlgorithm();
    void onClearHighlights();
    void onClearAll();
    void onResetLayout();
    void onUndo();
    void onRedo();
    void onPrevSolution();
    void onNextSolution();
    void onCheckForUpdates();
    void onOpenDownloadPage();
    void onOpenManual();
    void onAbout();

private:
    QWidget* createInputPanel();
    QWidget* createControlBar();
    void loadSampleGraph();
    void updateStatus(const QString& msg);
    void applyHamiltonHighlight(const std::vector<std::string>& nodes, bool isCircuit);
    void applyEulerHighlight(const std::vector<std::string>& nodes, bool isCircuit);
    void clearSolutionButtons();

    Graph* m_graph;
    GraphWidget* m_graphWidget;
    GraphTextEdit* m_textEdit;
    QComboBox* m_algoCombo;
    QLineEdit* m_fromEdit;
    QLineEdit* m_toEdit;
    QLabel* m_statusLabel;
    QPushButton* m_btnPrevSolution = nullptr;
    QPushButton* m_btnNextSolution = nullptr;
    UpdateChecker* m_updateChecker;
    HamiltonResult m_lastHamiltonResult;
    bool m_hasHamiltonResult = false;
    EulerResult m_lastEulerResult;
    bool m_hasEulerResult = false;
};

#endif // MAINWINDOW_H
