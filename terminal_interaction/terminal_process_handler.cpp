#include "terminal_interaction/terminal_process_handler.h" // Adjusted
#include <QDebug>
#include <QFileInfo> // For working directory example

TerminalProcessHandler::TerminalProcessHandler(QObject *parent)
    : QObject(parent), m_process(new QProcess(this)), m_isShellMode(false) {
    connect(m_process, &QProcess::readyReadStandardOutput, this, &TerminalProcessHandler::onReadyReadStandardOutput);
    connect(m_process, &QProcess::readyReadStandardError, this, &TerminalProcessHandler::onReadyReadStandardError);
    connect(m_process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this, &TerminalProcessHandler::onProcessFinished); // Explicit overload
    connect(m_process, &QProcess::errorOccurred, this, &TerminalProcessHandler::onProcessError);
    connect(m_process, &QProcess::started, this, &TerminalProcessHandler::onProcessStarted);
}

TerminalProcessHandler::~TerminalProcessHandler() {
    stopProcess();
}

bool TerminalProcessHandler::startShell(const QString& programPath, const QStringList& arguments) {
    if (m_process->state() != QProcess::NotRunning) {
        qWarning() << "Process is already running.";
        return false;
    }
    m_isShellMode = true;
    m_process->setProgram(programPath);
    m_process->setArguments(arguments);
    // For Windows, ensure we handle paths correctly if bash expects MSYS/Cygwin paths
    // QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    // env.insert("MSYS", "winsymlinks:nativestrict"); // Example, might not be needed for all setups
    // m_process->setProcessEnvironment(env);

    // Setting a working directory can be important for shells
    // QFileInfo programInfo(programPath);
    // QString msysRoot = programInfo.dir().dir().path(); // Example: C:/msys64 if bash is in C:/msys64/usr/bin
    // m_process->setWorkingDirectory(msysRoot); // Or let it inherit

    m_process->start();
    if (!m_process->waitForStarted(5000)) { // Increased timeout
        qWarning() << "Shell process failed to start:" << m_process->errorString();
        return false;
    }
    return true;
}

bool TerminalProcessHandler::executeSingleCommand(const QString& programPath, const QStringList& arguments) {
    if (m_process->state() != QProcess::NotRunning) {
        qWarning() << "Process is already running. Cannot execute single command.";
        return false;
    }
    m_isShellMode = false;
    m_process->setProgram(programPath);
    m_process->setArguments(arguments);
    m_process->start();
     if (!m_process->waitForStarted(5000)) {
        qWarning() << "Single command process failed to start:" << m_process->errorString();
        return false;
    }
    return true;
}

void TerminalProcessHandler::sendCommandToShell(const QString& command) {
    if (m_process->state() == QProcess::Running && m_isShellMode) {
        QByteArray commandData = command.toUtf8();
        if (!commandData.endsWith('\n')) {
            commandData.append('\n');
        }
        qint64 bytesWritten = m_process->write(commandData);
        if (bytesWritten == -1) {
            qWarning() << "Failed to write to process stdin:" << m_process->errorString();
        }
        // m_process->waitForBytesWritten(1000); // Can be blocking
    } else {
        qWarning() << "Cannot send command: Process not running or not in shell mode.";
    }
}

void TerminalProcessHandler::stopProcess() {
    if (m_process->state() != QProcess::NotRunning) {
        m_process->closeWriteChannel(); // Important for shells waiting for EOF on stdin
        m_process->terminate();
        if (!m_process->waitForFinished(3000)) {
            qDebug() << "Process did not terminate gracefully, killing.";
            m_process->kill();
            m_process->waitForFinished(1000);
        }
    }
}

bool TerminalProcessHandler::isRunning() const {
    return m_process->state() == QProcess::Running;
}

QString TerminalProcessHandler::errorString() const {
    return m_process->errorString();
}

void TerminalProcessHandler::onReadyReadStandardOutput() {
    QByteArray data = m_process->readAllStandardOutput();
    emit outputReady(QString::fromUtf8(data));
}

void TerminalProcessHandler::onReadyReadStandardError() {
    QByteArray data = m_process->readAllStandardError();
    emit errorReady(QString::fromUtf8(data));
}

void TerminalProcessHandler::onProcessStarted() {
    emit processStarted();
}

void TerminalProcessHandler::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus) {
    if (m_process->bytesAvailable() > 0) { onReadyReadStandardOutput(); }
    if (m_process->bytesAvailableError() > 0) { onReadyReadStandardError(); }
    emit processFinished(exitCode, exitStatus);
    // m_isShellMode = false; // Don't reset here if shell could be restarted by user
}

void TerminalProcessHandler::onProcessError(QProcess::ProcessError error) {
    // errorOccurred is often emitted before finished() when a process fails to start.
    qDebug() << "QProcess errorOccurred:" << error << m_process->errorString();
    emit errorReady(m_process->errorString()); // Emit the error string
    emit processError(error); // Emit the enum
    // m_isShellMode = false;
}