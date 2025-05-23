#ifndef ORCHESTRATOR_MANAGER_H
#define ORCHESTRATOR_MANAGER_H

#include <QObject>
#include <QStringList>
#include <QString>
#include <vector>
#include <functional>

class LLMService;
namespace CommandLibrary { class CommandManager; } // Forward declare with namespace
struct ConversationTurn;
class MainWindow;

class OrchestratorManager : public QObject {
    Q_OBJECT
public:
    explicit OrchestratorManager(MainWindow* mainWindow, LLMService* taskLlmService, CommandLibrary::CommandManager* commandManager, QObject* parent = nullptr);
    void setTaskLlmService(LLMService* service);
    bool startOrchestration(const QStringList& todoList);
    void stopOrchestration();
    // void pauseOrchestration();
    // void resumeOrchestration();
    bool isRunning() const { return m_isRunning; }

signals:
    void orchestrationStatusUpdate(const QString& status);
    void orchestratorNeedsUserInput(const QString& promptForUser);
    void orchestratorSuggestsLocalCommand(const QString& command, const QString& explanation);
    void orchestrationFinished(bool success, const QString& summary);
    void orchestratorDisplayMessage(const QString& role, const QString& message);

public slots:
    void processTaskLlmResponse(const QString& responseText, const QString& requestPayload, const QString& fullResponsePayload, const QString& error);
    void processLocalCommandResult(const QString& commandName, const QString& args, const QString& result, bool success);

private:
    void nextStep();
    void executeCurrentStep();
    QString generatePromptForTaskLLM(const QString& currentTodoItem, const QString& previousContext);
    bool interpretTaskLLMResponse(const QString& response, QString& outSuggestedCommand, QString& outNextInstructionForUser);
    bool isStepCompleted(const QString& todoItem, const QString& llmResponse, const QString& commandResult);

    MainWindow* m_mainWindow;
    LLMService* m_taskLlmService;
    CommandLibrary::CommandManager* m_commandManager;
    QStringList m_todoList;
    int m_currentStepIndex;
    bool m_isRunning;
    bool m_isPaused;
    bool m_waitingForCommandConfirmation;
    QString m_lastCommandResult;
    QString m_lastLlmResponse;
    std::vector<ConversationTurn> m_currentStepConversationHistory;
};
#endif // ORCHESTRATOR_MANAGER_H