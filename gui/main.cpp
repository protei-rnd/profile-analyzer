#include <QApplication>
#include <QDesktopWidget>

#include "mainwindow.h"

int main(int argc, char *argv[]) {
    QCoreApplication::setOrganizationName("Protei");
    QCoreApplication::setApplicationName("profile_analyzer");

    QApplication a(argc, argv);
    MainWindow w;

    w.show();

    return a.exec();
}
