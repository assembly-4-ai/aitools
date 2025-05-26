#ifndef SESSION_LOGGER_H
#define SESSION_LOGGER_H

#include <QString>
#include <memory> // For std::unique_ptr

// Forward declarations
class QFile;
class QTextStream;

class SessionLogger {
public:
    // Constructor takes the base directory where 'chat_logs' subdirectory will be created/used.
    explicit SessionLogger(const QString& baseLogDirPath);
    ~SessionLogger(); // To ensure file is closed properly if open

    // Starts a new log file for a new session.
    // Returns true on success, false on failure.
    bool startNewSessionLog();

    // Logging methods
    void logUserPrompt(const QString& promptText);
    void logLlmRequest(const QString& serviceName, const QString& modelName, const QString& requestPayload);
    void logLlmResponse(const QString& serviceName, const QString& modelName, 
                        const QString& responseText, const QString& fullResponsePayload, 
                        const QString& errorString); // errorString can be empty
    void logLocalCommand(const QString& command, const QString& resultOutput, 
                         const QString& resultError, bool userApproved);
    void logInfo(const QString& message);
    void logError(const QString& message);

private:
    void closeCurrentLogFile(); // Helper to close and nullify current file/stream
    void writeLogEntry(const QString& type, const QString& message, const QString& details = QString());

    QString m_logDirectory;      // Full path to the 'chat_logs' directory
    QString m_currentLogFilePath; 
    
    std::unique_ptr<QFile> m_currentLogFile;
    std::unique_ptr<QTextStream> m_logStream;
};

#endif // SESSION_LOGGER_H
