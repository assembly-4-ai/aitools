// openai_compatible_service.h

#ifndef OPENAI_COMPATIBLE_SERVICE_H
#define OPENAI_COMPATIBLE_SERVICE_H

#include "llm_service.h" // Assuming llm_service.h is in the same directory or reachable
#include <QFutureWatcher> // For async HTTP calls
#include <QVariantMap>    // For returning multiple values from async task (request/response payloads)

// Forward declaration of ConversationTurn if it's not included via llm_service.h
// struct ConversationTurn; // Uncomment if needed

class OpenAICompatibleService : public LLMService {
    Q_OBJECT

public:
    // Pass a display name (e.g., "LM Studio", "VLLM") and its typical default URL
    explicit OpenAICompatibleService(const QString& serviceDisplayName,
                                     const QString& defaultBaseUrl,
                                     QObject* parent = nullptr);

    // --- LLMService Interface Implementation ---
    QString getName() const override; // Will return m_serviceDisplayName
    bool isConfigured() override;
    bool configureService(QWidget* parentWindow) override;

    // Callback: models, errorString (empty if success)
    void fetchModels(std::function<void(QStringList, QString)> callback) override;

    // Callback: response_text, full_request_payload, full_response_payload, errorString
    void generateResponse(
        const std::vector<ConversationTurn>& history,
        const QString& systemPrompt,
        const QString& modelName,
        std::function<void(QString, QString, QString, QString)> callback) override;

private:
    QString m_serviceDisplayName; // e.g., "LM Studio", "VLLM"
    QString m_defaultBaseUrl;     // e.g., "http://localhost:1234/v1"
    QString m_baseUrl;            // The currently configured base URL
    // m_lastSelectedModel is inherited from LLMService

    // Async helpers for HTTP calls
    QFutureWatcher<QPair<QStringList, QString>> m_modelFetchWatcher;
    std::function<void(QStringList, QString)> m_modelFetchCallback;

    QFutureWatcher<QVariantMap> m_generateWatcher; // For generateResponse
    std::function<void(QString, QString, QString, QString)> m_generateCallback;

    QString getSettingsKeyBase() const; // Helper for QSettings keys

private slots:
    // Slots to handle the results from the QFutureWatchers
    void handleModelFetchResponse();
    void handleGenerateResponse();
};

#endif // OPENAI_COMPATIBLE_SERVICE_H