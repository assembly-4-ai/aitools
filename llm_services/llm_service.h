#ifndef LLM_SERVICE_H
#define LLM_SERVICE_H

#include <QString>
#include <QStringList>
#include <vector>
#include <functional>
#include <QObject>

struct ConversationTurn; // Forward declaration

class LLMService : public QObject {
    Q_OBJECT
public:
    explicit LLMService(QObject* parent = nullptr);
    virtual ~LLMService() = default;

    virtual QString getName() const = 0;
    virtual bool isConfigured() = 0;
    virtual bool configureService(QWidget* parentWindow) = 0;
    virtual void fetchModels(std::function<void(QStringList, QString)> callback) = 0;
    virtual void generateResponse(
        const std::vector<ConversationTurn>& history,
        const QString& systemPrompt,
        const QString& modelName,
        std::function<void(QString, QString, QString, QString)> callback) = 0; // text, req_payload, resp_payload, error
    virtual QString getLastSelectedModel() const;
    virtual void setLastSelectedModel(const QString& modelName);

protected:
    QString m_lastSelectedModel;
};
#endif // LLM_SERVICE_H