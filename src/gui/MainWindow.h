#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

class Graph;
class GraphWidget;
class QPlainTextEdit;
class QComboBox;
class QLineEdit;
class QLabel;

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

private:
    QWidget* createInputPanel();
    QWidget* createControlBar();
    void loadSampleGraph();
    void updateStatus(const QString& msg);

    Graph* m_graph;
    GraphWidget* m_graphWidget;
    QPlainTextEdit* m_textEdit;
    QComboBox* m_algoCombo;
    QLineEdit* m_fromEdit;
    QLineEdit* m_toEdit;
    QLabel* m_statusLabel;
};

#endif // MAINWINDOW_H
