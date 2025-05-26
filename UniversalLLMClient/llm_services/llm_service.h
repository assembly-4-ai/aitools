#ifndef LLM_SERVICE_H
#define LLM_SERVICE_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <vector>
#include "common/types.h" // For Common::ConversationTurn

class QWidget; // Forward declaration for parent arguments

class LLMService : public QObject {
    Q_OBJECT

public:
    explicit LLMService(QObject *parent = nullptr) : QObject(parent) {}
    virtual ~LLMService() = default;

    // Pure virtual functions to be implemented by concrete services
    virtual QString getName() const = 0;
    virtual bool isConfigured() = 0;
    virtual void configureService(QWidget* parent) = 0; // Parent for dialogs
    
    // Asynchronously fetches models. Emits modelsFetched signal.
    virtual void fetchModels(QWidget* parent) = 0; 

    // Asynchronously generates a response. Emits responseReady signal.
    // Takes current history and the new prompt text.
    // Parent QWidget can be used for any dialogs the service might need to show (e.g. errors during call setup)
    virtual void generateResponse(const std::vector<Common::ConversationTurn>& history, 
                                  const QString& prompt, 
                                  QWidget* parent) = 0;

    // Virtual getter/setter for last selected model
    virtual QString getLastSelectedModel() const { return m_lastSelectedModel; }
    virtual void setLastSelectedModel(const QString& modelName) { m_lastSelectedModel = modelName; }

signals:
    // Emitted when a response is generated or an error occurs
    void responseReady(const QString& responseText, 
                       const QString& requestPayload, 
                       const QString& fullResponsePayload, 
                       const QString& errorString);

    // Emitted when model fetching is complete or an error occurs
    void modelsFetched(const QStringList& modelList, const QString& errorString);

protected:
    QString m_lastSelectedModel;
};

#endif // LLM_SERVICE_H
