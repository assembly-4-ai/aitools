#ifndef OPENAI_COMPATIBLE_SERVICE_H
#define OPENAI_COMPATIBLE_SERVICE_H

#include "llm_services/llm_service.h"
#include <QFutureWatcher>
#include <QVariantMap>
#include <QStringList>
#include <QPair>

class QSettings; // Forward declaration

class OpenAICompatibleService : public LLMService {
    Q_OBJECT

public:
    // Constructor takes a display name for the service (e.g., "LM Studio", "My VLLM Server")
    // and a default base URL for initial configuration.
    explicit OpenAICompatibleService(const QString& serviceDisplayName, 
                                     const QString& defaultBaseUrl, 
                                     QObject *parent = nullptr);
    ~OpenAICompatibleService() override = default;

    // Override pure virtual functions from LLMService
    QString getName() const override; // Will return m_serviceDisplayName
    bool isConfigured() override;
    void configureService(QWidget* parent) override;
    void fetchModels(QWidget* parent) override;
    void generateResponse(const std::vector<Common::ConversationTurn>& history, 
                          const QString& prompt, 
                          QWidget* parent) override;

private slots:
    void handleGenerateFinished();
    void handleFetchModelsFinished();

private:
    void loadSettings(); // Settings will be keyed by m_serviceSettingsKey
    void saveSettings(); // Settings will be keyed by m_serviceSettingsKey
    QString sanitizeServiceNameForKey(const QString& name) const; // Helper to create a QSettings-friendly key

    QString m_serviceDisplayName; // User-facing name (e.g., "LM Studio")
    QString m_defaultBaseUrl;     // Default URL provided at construction
    QString m_baseUrl;            // Actual base URL configured by user
    QString m_apiKey;             // API key, if required by the service

    QString m_serviceSettingsKey; // Derived from m_serviceDisplayName for QSettings

    QFutureWatcher<QVariantMap> m_generateWatcher;
    QFutureWatcher<QPair<QStringList, QString>> m_fetchModelsWatcher;
    
    QWidget* m_dialogParent; 
};

#endif // OPENAI_COMPATIBLE_SERVICE_H
