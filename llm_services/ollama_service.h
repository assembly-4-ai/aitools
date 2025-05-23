// ollama_service.h

#ifndef OLLAMA_SERVICE_H
#define OLLAMA_SERVICE_H

#include "llm_service.h" // Assuming llm_service.h is in the same directory or reachable
#include <QFutureWatcher> // For async HTTP calls
#include <QPair>          // For returning multiple values from async task

// Forward declaration of ConversationTurn if it's not included via llm_service.h
// struct ConversationTurn; // Uncomment if needed and ConversationTurn is defined elsewhere

class OllamaService : public LLMService {
    Q_OBJECT // Required for any QObject-derived class with signals/slots

public:
    explicit OllamaService(QObject* parent = nullptr);

    // --- LLMService Interface Implementation ---
    QString getName() const override;
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

    // getLastSelectedModel() and setLastSelectedModel() are inherited from LLMService

private:
    QString m_baseUrl; // e.g., http://localhost:11434
    // m_lastSelectedModel is inherited from LLMService

    // Async helpers for HTTP calls
    QFutureWatcher<QPair<QStringList, QString>> m_modelFetchWatcher;
    std::function<void(QStringList, QString)> m_modelFetchCallback;

    // QFutureWatcher for generateResponse (modified to carry more data)
    // The QPair will now hold: {response_text, {request_payload, full_response_payload}}
    // And a separate string for error. Or use a QVariantMap for more flexibility.
    // Let's use QVariantMap as it's cleaner for multiple return values.
    QFutureWatcher<QVariantMap> m_generateWatcher;
    std::function<void(QString, QString, QString, QString)> m_generateCallback;


private slots:
    // Slots to handle the results from the QFutureWatchers
    void handleModelFetchResponse();
    void handleGenerateResponse(); // Will now handle QVariantMap
};

#endif // OLLAMA_SERVICE_H