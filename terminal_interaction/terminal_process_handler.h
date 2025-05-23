#ifndef TERMINAL_PROCESS_HANDLER_H
#define TERMINAL_PROCESS_HANDLER_H

#include <QObject>
#include <QProcess>
#include <QString>

class TerminalProcessHandler : public QObject {
    Q_OBJECT

public:
    explicit TerminalProcessHandler(QObject *parent = nullptr);
    ~TerminalProcessHandler();

    bool startShell(const QString& programPath, const QStringList& arguments = QStringList());
    bool executeSingleCommand(const QString& programPath, const QStringList& arguments);
    void sendCommandToShell(const QString& command);
    void stopProcess();
    bool isRunning() const;
    QString errorString() const; // Helper to get QProcess error string

signals:
    void outputReady(const QString& output);
    void errorReady(const QString& errorOutput);
    void processStarted();
    void processFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void processError(QProcess::ProcessError error); // QProcess's error enum

private slots:
    void onReadyReadStandardOutput();
    void onReadyReadStandardError();
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onProcessError(QProcess::ProcessError error);
    void onProcessStarted();

private:
    QProcess *m_process;
    bool m_isShellMode;
};
#endif // TERMINAL_PROCESS_HANDLER_H