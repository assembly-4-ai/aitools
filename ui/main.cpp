#include "ui/mainwindow.h" // Adjusted path
#include <QApplication>
#include <QDir>
#include <QStandardPaths>

int main(int argc, char *argv[]) {
    QApplication a(argc, argv);
    QApplication::setApplicationName("UniversalLLMClient");
    QApplication::setOrganizationName("MyCompany"); // For QSettings

    // Ensure base app data directory exists for logs, etc.
    QString appDataPath = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    QDir appDataDir(appDataPath);
    if (!appDataDir.exists()) {
        appDataDir.mkpath(".");
    }

    MainWindow w;
    w.show();
    return a.exec();
}