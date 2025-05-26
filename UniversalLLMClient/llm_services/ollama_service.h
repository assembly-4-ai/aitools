#ifndef OLLAMA_SERVICE_H
#define OLLAMA_SERVICE_H

#include "llm_services/llm_service.h" // Base class
#include <QFutureWatcher>
#include <QVariantMap>  // For generateResponse results
#include <QStringList>  // For fetchModels results
#include <QPair>        // For fetchModels results (models, error string)

class QSettings; // Forward declaration

class OllamaService : public LLMService {
    Q_OBJECT

public:
    explicit OllamaService(QObject *parent = nullptr);
    ~OllamaService() override = default;

    // Override pure virtual functions from LLMService
    QString getName() const override;
    bool isConfigured() override;
    void configureService(QWidget* parent) override;
    void fetchModels(QWidget* parent) override;
    void generateResponse(const std::vector<Common::ConversationTurn>& history, 
                          const QString& prompt, 
                          QWidget* parent) override;

private slots:
    void handleGenerateFinished(); // Slot for m_generateWatcher
    void handleFetchModelsFinished(); // Slot for m_fetchModelsWatcher

private:
    void loadSettings();
    void saveSettings();

    QString m_baseUrl; // e.g., http://localhost:11434

    QFutureWatcher<QVariantMap> m_generateWatcher;
    QFutureWatcher<QPair<QStringList, QString>> m_fetchModelsWatcher;
    
    QWidget* m_dialogParent; // To store parent for dialogs if needed by async callbacks
};

#endif // OLLAMA_SERVICE_H
