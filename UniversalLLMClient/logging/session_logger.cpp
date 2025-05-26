#include "session_logger.h"
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QDir>
#include <QStandardPaths> 
#include <iostream> 

SessionLogger::SessionLogger(const QString& baseLogDirPath) {
    QDir baseDir(baseLogDirPath);
    if (!baseDir.exists()) {
        if (!baseDir.mkpath(".")) { 
            std::cerr << "SessionLogger: Failed to create base log directory: " << baseLogDirPath.toStdString() << std::endl;
        }
    }

    m_logDirectory = baseLogDirPath + "/chat_logs"; // Ensure this path is valid
    QDir logDir(m_logDirectory);
    if (!logDir.exists()) {
        if (!logDir.mkpath(".")) {
            std::cerr << "SessionLogger: Failed to create chat_logs directory: " << m_logDirectory.toStdString() << std::endl;
        }
    }
    // Initial log start is important
    if (!startNewSessionLog()) {
        std::cerr << "SessionLogger: Initial log session could not be started. Logging may not function." << std::endl;
    }
}

SessionLogger::~SessionLogger() {
    logInfo("Session ending."); // Log session end before closing
    closeCurrentLogFile();
}

void SessionLogger::closeCurrentLogFile() {
    if (m_logStream) {
        m_logStream->flush();
    }
    if (m_currentLogFile && m_currentLogFile->isOpen()) {
        m_currentLogFile->close();
    }
    m_logStream.reset();
    m_currentLogFile.reset();
    m_currentLogFilePath.clear();
}

bool SessionLogger::startNewSessionLog() {
    closeCurrentLogFile(); 

    // Check if m_logDirectory is usable, it might have failed in constructor
    if (m_logDirectory.isEmpty() || !QDir(m_logDirectory).exists()) {
         // Try to re-establish a fallback log directory if initial setup failed
        QString fallbackBase = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
        if (fallbackBase.isEmpty()) fallbackBase = "."; // Absolute last resort
        m_logDirectory = fallbackBase + "/UniversalLLMClient_fallback_logs/chat_logs";
        QDir tempLogDir(m_logDirectory);
        if (!tempLogDir.mkpath(".")) {
            std::cerr << "SessionLogger: Critical - Failed to create even fallback log directory: " << m_logDirectory.toStdString() << std::endl;
            return false;
        }
         std::cerr << "SessionLogger: Warning - Using fallback log directory: " << m_logDirectory.toStdString() << std::endl;
    }


    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
    m_currentLogFilePath = m_logDirectory + "/session_" + timestamp + ".log";

    m_currentLogFile = std::make_unique<QFile>(m_currentLogFilePath);
    if (!m_currentLogFile->open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Append)) { // Append is okay for safety
        std::cerr << "SessionLogger: Failed to open new log file: " << m_currentLogFilePath.toStdString() 
                  << " Error: " << m_currentLogFile->errorString().toStdString() << std::endl;
        m_currentLogFile.reset(); 
        return false;
    }

    m_logStream = std::make_unique<QTextStream>(m_currentLogFile.get());
    m_logStream->setEncoding(QStringConverter::Utf8); 

    writeLogEntry("INFO", "New session started.", QDateTime::currentDateTime().toString(Qt::ISODateWithMs));
    return true;
}

void SessionLogger::writeLogEntry(const QString& type, const QString& message, const QString& details) {
    if (!m_logStream || !m_currentLogFile || !m_currentLogFile->isOpen()) { // More robust check
        std::cerr << "SessionLogger: Log stream not available or file not open. Cannot write entry: [" << type.toStdString() << "] " << message.toStdString() << std::endl;
        return;
    }

    QString timestamp = QDateTime::currentDateTime().toString(Qt::ISODateWithMs);
    (*m_logStream) << "[" << timestamp << "] [" << type << "] " << message;
    if (!details.isEmpty()) {
        QString indentedDetails = details;
        indentedDetails.replace("\n", "\n    "); 
        (*m_logStream) << "\n    " << indentedDetails;
    }
    (*m_logStream) << "\n"; // Use Qt::endl or "\n" consistently
    m_logStream->flush(); 
}

void SessionLogger::logUserPrompt(const QString& promptText) {
    writeLogEntry("USER_PROMPT", "Prompt:", promptText);
}

void SessionLogger::logLlmRequest(const QString& serviceName, const QString& modelName, const QString& requestPayload) {
    writeLogEntry("LLM_REQUEST", 
                  QString("Service: %1, Model: %2").arg(serviceName, modelName),
                  requestPayload);
}

void SessionLogger::logLlmResponse(const QString& serviceName, const QString& modelName, 
                                   const QString& responseText, const QString& fullResponsePayload, 
                                   const QString& errorString) {
    if (!errorString.isEmpty()) {
        writeLogEntry("LLM_ERROR", 
                      QString("Service: %1, Model: %2, Error: %3").arg(serviceName, modelName, errorString),
                      fullResponsePayload.isEmpty() ? "No full payload on error." : fullResponsePayload);
    } else {
        writeLogEntry("LLM_RESPONSE", 
                      QString("Service: %1, Model: %2, Response (Preview): %3")
                          .arg(serviceName, modelName, responseText.left(150).replace("\n", " ")), // Slightly longer preview
                      fullResponsePayload); 
    }
}

void SessionLogger::logLocalCommand(const QString& command, const QString& resultOutput, 
                                    const QString& resultError, bool userApproved) {
    QString status = userApproved ? "Approved" : "Denied/Cancelled";
    if (!resultError.isEmpty()) {
        writeLogEntry("LOCAL_CMD_ERROR", 
                      QString("Command: %1, Status: %2, Error: %3").arg(command, status, resultError),
                      resultOutput.isEmpty() ? "No output on error." : resultOutput);
    } else {
        writeLogEntry("LOCAL_CMD", 
                      QString("Command: %1, Status: %2").arg(command, status),
                      resultOutput.isEmpty() ? "No output." : resultOutput);
    }
}

void SessionLogger::logInfo(const QString& message) {
    writeLogEntry("INFO", message);
}

void SessionLogger::logError(const QString& message) {
    writeLogEntry("ERROR", message);
}
