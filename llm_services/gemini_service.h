#ifndef GEMINI_SERVICE_H
#define GEMINI_SERVICE_H

#include "llm_services/llm_service.h" // Adjusted
#include <QFutureWatcher>
#include <QVariantMap> // For returning multiple values from async task

// ConversationTurn struct should be accessible (e.g. from mainwindow.h or a common header)
// If not, forward declare or include the definition. For this example, assume accessible.

class GeminiService : public LLMService {
    Q_OBJECT
public:
    explicit GeminiService(QObject* parent = nullptr);
    QString getName() const override { return "Google Gemini"; }
    bool isConfigured() override;
    bool configureService(QWidget* parentWindow) override;
    void fetchModels(std::function<void(QStringList, QString)> callback) override;
    void generateResponse(
        const std::vector<ConversationTurn>& history,
        const QString& systemPrompt,
        const QString& modelName,
        std::function<void(QString, QString, QString, QString)> callback) override;

private:
    QString m_apiKey;
    QString m_modelIdentifier;

    QFutureWatcher<QVariantMap> m_httpWatcher; // Changed to QVariantMap
    std::function<void(QString, QString, QString, QString)> m_generateCallback;

private slots:
    void handleHttpResponse();
};
#endif // GEMINI_SERVICE_H