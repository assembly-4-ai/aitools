#include <QApplication>
// Assuming mainwindow.h will be in the same ui directory.
// This will require mainwindow.h to exist and be correct.
#include "mainwindow.h" 

int main(int argc, char *argv[]) {
    QApplication a(argc, argv);

    // Set application details as per the original master prompt
    QCoreApplication::setOrganizationName("YourOrganizationName"); 
    QCoreApplication::setApplicationName("UniversalLLMClient");    
    QCoreApplication::setApplicationVersion("0.1");                 

    MainWindow w; 
    w.show();

    return a.exec();
}
