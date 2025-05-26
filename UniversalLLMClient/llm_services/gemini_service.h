#ifndef GEMINI_SERVICE_H
#define GEMINI_SERVICE_H

#include "llm_services/llm_service.h" // Base class
#include <QFutureWatcher>
#include <QVariantMap>
#include <QString>

class QSettings; // Forward declaration

// Namespace for Gemini-specific ConversationTurn serialization, if needed here.
// However, it was defined in mainwindow.cpp for Phase 1.
// It's better suited for gemini_service.cpp. For now, not needed in the header.
// namespace ns_turn_gemini { ... } 

class GeminiService : public LLMService {
    Q_OBJECT

public:
    explicit GeminiService(QObject *parent = nullptr);
    ~GeminiService() override = default;

    // Override pure virtual functions from LLMService
    QString getName() const override;
    bool isConfigured() override;
    void configureService(QWidget* parent) override;
    void fetchModels(QWidget* parent) override;
    void generateResponse(const std::vector<Common::ConversationTurn>& history, 
                          const QString& prompt, 
                          QWidget* parent) override;
    
    // Optionally, override getters/setters for m_lastSelectedModel if Gemini has specific logic
    // For now, using base class implementation.
    // QString getLastSelectedModel() const override;
    // void setLastSelectedModel(const QString& modelName) override;

private slots:
    void handleApiCallFinished(); // Slot for this service's QFutureWatcher

private:
    void loadSettings();
    void saveSettings();

    QString m_apiKey;
    QString m_modelIdentifier; // e.g., "gemini-pro"

    QFutureWatcher<QVariantMap> m_apiWatcher; // Each service manages its own watcher
    QWidget* m_dialogParent; // Store parent for dialogs shown from async parts if needed, or pass through
};

#endif // GEMINI_SERVICE_H
