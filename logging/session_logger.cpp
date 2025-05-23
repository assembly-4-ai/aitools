// logging/session_logger.cpp
#include "session_logger.h"
#include <QStandardPaths>
#include <QDebug>
#include <QCoreApplication> // <<< ADD THIS

SessionLogger::SessionLogger() {
    QDir logDirObj; // Use a local QDir object
    // Try AppLocalDataLocation first
    QString logsPath = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    if (logsPath.isEmpty()) { // Fallback if AppLocalDataLocation is not available
        qWarning() << "AppLocalDataLocation is empty, falling back to applicationDirPath for logs.";
        logsPath = QCoreApplication::applicationDirPath(); // Needs <QCoreApplication>
    }
    logsPath += QDir::separator() + LOG_DIR_NAME;

    if (!logDirObj.exists(logsPath)) {
        if (!logDirObj.mkpath(logsPath)) {
            qWarning() << "Could not create logs directory:" << logsPath << ". Falling back to current directory.";
            // Fallback to local directory relative to executable or current working directory
            logsPath = LOG_DIR_NAME; // Relative path
            if (!logDirObj.exists(logsPath) && !logDirObj.mkpath(logsPath)) {
                 qCritical() << "Critial: Could not create any log directory.";
            }
        }
    }
    // The actual file opening is in startNewSessionLog
}

// ... (rest of session_logger.cpp as provided before, it seemed mostly okay
//      except for the QCoreApplication::applicationDirPath() if AppLocalDataLocation failed)
// Make sure getCurrentLogFilePath() correctly uses the path determined in constructor or startNewSessionLog.
// The current version of startNewSessionLog already tries to construct the path correctly.