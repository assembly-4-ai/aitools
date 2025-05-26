#ifndef TERMINAL_PROCESS_HANDLER_H
#define TERMINAL_PROCESS_HANDLER_H

#include <QObject>
#include <QProcess> // For managing external processes
#include <QStringList>

class TerminalProcessHandler : public QObject {
    Q_OBJECT

public:
    explicit TerminalProcessHandler(QObject *parent = nullptr);
    ~TerminalProcessHandler() override;

    // Starts an interactive shell.
    // programPath: e.g., "bash", "cmd.exe", "/path/to/msys2_shell.cmd"
    // arguments: e.g., {"-l", "-i"} for bash login interactive shell
    // Returns true if process started, false otherwise.
    bool startShell(const QString& programPath, const QStringList& arguments = QStringList());

    // Executes a single command and waits for it to finish (or can be used non-blockingly too).
    // For conceptual phase, might not fully implement waiting logic here, relies on QProcess signals.
    // inputData can be written to the process's standard input.
    // Returns true if process started.
    bool executeSingleCommand(const QString& programPath, 
                              const QStringList& arguments = QStringList(), 
                              const QByteArray& inputData = QByteArray());

    // Sends a command string to the running shell (must be in interactive shell mode).
    // Appends a newline character to the command.
    bool sendCommandToShell(const QString& command);

    // Stops the currently running process (shell or single command).
    void stopProcess();
    
    QProcess::ProcessState state() const;


signals:
    void outputReady(const QString& output); // Emits data from standard output
    void errorReady(const QString& errorOutput);  // Emits data from standard error
    void processFinished(int exitCode, QProcess::ExitStatus exitStatus); // Emitted when process finishes
    void processErrorOccurred(QProcess::ProcessError error); // Emitted on QProcess errors

private slots:
    void onProcessReadyReadStandardOutput();
    void onProcessReadyReadStandardError();
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus); // Slot for QProcess::finished
    void onProcessErrorOccurred(QProcess::ProcessError error);      // Slot for QProcess::errorOccurred

private:
    QProcess m_process;
    bool m_isShellMode; // To track if sendCommandToShell is appropriate
};

#endif // TERMINAL_PROCESS_HANDLER_H
