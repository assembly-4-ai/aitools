#ifndef SESSION_LOGGER_H
#define SESSION_LOGGER_H

#include <QString>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QDir>

class SessionLogger {
public:
    SessionLogger();
    ~SessionLogger();

    bool startNewSessionLog(const QString& sessionNamePrefix = "chat_session");
    void logUserPrompt(const QString& prompt);
    void logLlmRequest(const QString& serviceName, const QString& modelName, const QString& requestPayload);
    void logLlmResponse(const QString& serviceName, const QString& modelName, const QString& responseText, const QString& fullResponsePayload = "");
    void logLocalCommand(const QString& command, const QString& args, const QString& result);
    void logInfo(const QString& message);
    void logError(const QString& errorMessage);
    QString getCurrentLogFilePath() const;

private:
    QFile logFile;
    QTextStream logStream;
    QString currentLogFilePath;
    bool isLoggingActive = false;
    const QString LOG_DIR_NAME = "chat_logs";
};
#endif // SESSION_LOGGER_H