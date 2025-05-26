#include "terminal_process_handler.h"
#include <QDebug> 
#include <QDir> // For QDir::toNativeSeparators if needed, or general path work

TerminalProcessHandler::TerminalProcessHandler(QObject *parent) 
    : QObject(parent), m_isShellMode(false) {
    connect(&m_process, &QProcess::readyReadStandardOutput, 
            this, &TerminalProcessHandler::onProcessReadyReadStandardOutput);
    connect(&m_process, &QProcess::readyReadStandardError, 
            this, &TerminalProcessHandler::onProcessReadyReadStandardError);
    connect(&m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &TerminalProcessHandler::onProcessFinished);
    connect(&m_process, &QProcess::errorOccurred,
            this, &TerminalProcessHandler::onProcessErrorOccurred);
}

TerminalProcessHandler::~TerminalProcessHandler() {
    stopProcess(); 
}

bool TerminalProcessHandler::startShell(const QString& programPath, const QStringList& arguments) {
    if (m_process.state() != QProcess::NotRunning) {
        qWarning("TerminalProcessHandler::startShell: Process already running. Stop it first or wait.");
        return false;
    }
    m_isShellMode = true;
    m_process.setProgram(programPath);
    m_process.setArguments(arguments);
    
    // Example of setting working directory if needed:
    // QString workingDir = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    // if (!workingDir.isEmpty()) m_process.setWorkingDirectory(workingDir);

    m_process.start(); // QProcess::Unbuffered | QProcess::ReadWrite for more control if needed
    
    // It's better to rely on signals (started(), errorOccurred()) than waitForStarted() in a GUI app
    // For simplicity here, we'll return based on current state after start attempt.
    // A short waitForStarted might be acceptable if quick feedback is essential.
    if (!m_process.waitForStarted(1000)) { // Wait 1 second
        qWarning() << "Shell process failed to start or timed out:" << programPath << m_process.errorString();
        onProcessErrorOccurred(m_process.error()); // Manually trigger error signal if waitForStarted fails
        return false;
    }
    return true;
}

bool TerminalProcessHandler::executeSingleCommand(const QString& programPath, 
                                                  const QStringList& arguments, 
                                                  const QByteArray& inputData) {
    if (m_process.state() != QProcess::NotRunning) {
        qWarning("TerminalProcessHandler::executeSingleCommand: Process already running.");
        return false;
    }
    m_isShellMode = false;
    m_process.setProgram(programPath);
    m_process.setArguments(arguments);
    m_process.start();

    if (!m_process.waitForStarted(3000)) { 
        qWarning() << "Single command process failed to start:" << programPath << m_process.errorString();
        onProcessErrorOccurred(m_process.error());
        return false;
    }

    if (!inputData.isEmpty()) {
        m_process.write(inputData);
        m_process.closeWriteChannel(); 
    }
    // Output and finish will be handled by signals.
    return true;
}

bool TerminalProcessHandler::sendCommandToShell(const QString& command) {
    if (!m_isShellMode) {
        qWarning("TerminalProcessHandler::sendCommandToShell: Not in shell mode.");
        return false;
    }
    if (m_process.state() != QProcess::Running) {
        qWarning("TerminalProcessHandler::sendCommandToShell: Shell process not running.");
        return false;
    }

    QByteArray commandData = command.toUtf8();
    // Shells typically need a newline to execute a command line.
    if (!command.endsWith('\n') && !command.endsWith("\r\n")) { // Check for both just in case
        commandData.append('\n');
    }
    
    qint64 bytesWritten = m_process.write(commandData);
    if (bytesWritten == -1) {
        qWarning() << "TerminalProcessHandler::sendCommandToShell: Write error:" << m_process.errorString();
        return false;
    }
    // m_process.waitForBytesWritten(100); // Optional short wait
    return true;
}

void TerminalProcessHandler::stopProcess() {
    if (m_process.state() != QProcess::NotRunning) {
        qDebug("TerminalProcessHandler: Stopping process...");
        m_process.terminate(); 
        if (!m_process.waitForFinished(1000)) { 
            qWarning("TerminalProcessHandler: Process terminate failed or timed out, killing.");
            m_process.kill(); 
            m_process.waitForFinished(500); 
        }
        // m_isShellMode = false; // This will be set in onProcessFinished or onProcessErrorOccurred
        qDebug() << "TerminalProcessHandler: stopProcess attempted. Current State:" << m_process.state();
    }
}

QProcess::ProcessState TerminalProcessHandler::state() const {
    return m_process.state();
}

void TerminalProcessHandler::onProcessReadyReadStandardOutput() {
    QByteArray data = m_process.readAllStandardOutput();
    emit outputReady(QString::fromUtf8(data)); 
}

void TerminalProcessHandler::onProcessReadyReadStandardError() {
    QByteArray data = m_process.readAllStandardError();
    emit errorReady(QString::fromUtf8(data)); 
}

void TerminalProcessHandler::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus) {
    // Ensure all remaining output is read before emitting finished
    // This can be important if the process writes and exits quickly.
    QString remainingStdout = QString::fromUtf8(m_process.readAllStandardOutput());
    if(!remainingStdout.isEmpty()) emit outputReady(remainingStdout);
    QString remainingStderr = QString::fromUtf8(m_process.readAllStandardError());
    if(!remainingStderr.isEmpty()) emit errorReady(remainingStderr);

    qDebug() << "TerminalProcessHandler: Process finished. Exit code:" << exitCode << "Status:" << exitStatus;
    emit processFinished(exitCode, exitStatus);
    m_isShellMode = false; 
}

void TerminalProcessHandler::onProcessErrorOccurred(QProcess::ProcessError error) {
    qWarning() << "TerminalProcessHandler: Process error occurred:" << error << "-" << m_process.errorString();
    emit processErrorOccurred(error); 
    emit errorReady(m_process.errorString()); 
    m_isShellMode = false; 
}
