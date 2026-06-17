#include <QApplication>
#include <QNetworkProxyFactory>
#include "MainWindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QNetworkProxyFactory::setUseSystemConfiguration(true);
    app.setApplicationName("GraphViz");
    app.setApplicationVersion(GRAPHVIZ_VERSION);

    MainWindow window;
    window.show();

    return app.exec();
}
