#include "main_window.hpp"

#include "mediatool/bundle.hpp"

#include <QApplication>
#include <QFont>
#include <QIcon>
#include <QStyleFactory>

#include <filesystem>

int main(int argc, char* argv[]) {
    QApplication application(argc, argv);
    QApplication::setStyle(QStyleFactory::create("Fusion"));
    QApplication::setApplicationName("媒体小工具");
    QApplication::setQuitOnLastWindowClosed(false);
    QApplication::setFont(QFont("Microsoft YaHei UI", 14));
    const QIcon icon(":/icons/app.png");
    QApplication::setWindowIcon(icon);
    const auto bundle = MediaTool::locateBundle(std::filesystem::path(QCoreApplication::applicationDirPath().toStdWString()));
    MainWindow window(bundle);
    window.setWindowIcon(icon);
    window.show();
    return application.exec();
}
