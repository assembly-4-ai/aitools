#ifndef ORCHESTRATOR_MANAGER_H
#define ORCHESTRATOR_MANAGER_H

#include <QObject>
#include <QStringList>
#include <QFutureWatcher> // For its own LLM calls
#include <QVariantMap>    // For results from its LLM calls

// Forward declarations to avoid full includes in header
class MainWindow; // Assuming MainWindow.h is not included to prevent circular dependencies
class LLMService; // Defined in llm_services/llm_service.h
namespace CommandLib { class CommandManager; } // Defined in command_lib/command_library.h

class OrchestratorManager : public QObject {
    Q_OBJECT

public:
    // Constructor takes pointers to main application components.
    // Ownership of these pointers is not taken by OrchestratorManager.
    explicit OrchestratorManager(MainWindow* mainWindow, 
                                 LLMService* llmServiceForOrchestrator, 
                                 CommandLib::CommandManager* commandManager, 
                                 QObject *parent = nullptr);
    ~OrchestratorManager() override = default;

    // Public enum for state
    enum class OrchestrationState { Idle, Running, Paused, WaitingForUser, WaitingForCommand };

    // Public methods to control orchestration
    Q_INVOKABLE void startOrchestration(const QStringList& todoList);
    Q_INVOKABLE void stopOrchestration();

    // Method for MainWindow to feed back user input
    Q_INVOKABLE void provideUserInput(const QString& input);

    // Method for MainWindow to feed back command execution results
    Q_INVOKABLE void processExternalCommandResult(const QString& commandName, const QString& output, const QString& error);

    // New public methods
    OrchestrationState getState() const; 
    bool isRunning() const; // Convenience method
    bool hasValidLlmService() const;


signals:
    // To update MainWindow's orchestrator console or status bar
    void orchestrationStatusUpdate(const QString& statusMessage);

    // When the orchestrator needs input from the user to proceed
    // (e.g., "What is the filename?", "Should I proceed with X?")
    // suggestedResponse can be a default value for an input dialog.
    void orchestratorNeedsUserInput(const QString& question, const QString& suggestedResponse = QString());

    // When the orchestrator wants to execute a local command (MainWindow should confirm with user)
    void orchestratorSuggestsLocalCommand(const QString& commandToExecute);

    // When the entire orchestration task list is completed or terminally failed
    void orchestrationFinished(bool success, const QString& summary);

    // For general messages to be displayed in orchestrator's console (e.g., errors, LLM thoughts)
    void orchestratorDisplayMessage(const QString& message, bool isError = false);

private slots:
    // Slot to handle responses from the LLMService used by the orchestrator itself
    void processInternalLlmResponse(const QString& responseText, const QString& requestPayload, 
                                    const QString& fullResponsePayload, const QString& errorString);
    
    // Slot to handle results from its own QFutureWatcher for LLM calls
    void handleLlmFutureFinished();


private:
    // Internal methods for orchestration logic (conceptual stubs for Phase 6)
    void nextStep();
    void executeCurrentTask();
    void generatePromptForTaskLLM(const QString& taskDescription); // For asking LLM how to do a task
    void interpretLlmResponseForSubtasks(const QString& llmResponse); // Parses LLM output

    MainWindow* m_mainWindow; // Non-owning pointer
    LLMService* m_llmService; // Non-owning pointer to the LLM service it uses for its "thinking"
    CommandLib::CommandManager* m_commandManager; // Non-owning pointer

    QStringList m_todoList;
    int m_currentTaskIndex;
    // OrchestrationState enum moved to public
    OrchestrationState m_state;

    // QFutureWatcher for LLM calls made by the orchestrator for its planning/execution
    QFutureWatcher<QVariantMap> m_llmTaskWatcher; 
    
    // Temporary storage for pending user input or command details
    QString m_pendingQuestionId; // If multiple user inputs can be pending
};

#endif // ORCHESTRATOR_MANAGER_H
