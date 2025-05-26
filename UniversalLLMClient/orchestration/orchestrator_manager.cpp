#include "orchestrator_manager.h"
#include "llm_services/llm_service.h" // For LLMService interface
#include "command_lib/command_library.h" // For CommandManager interface
// #include "ui/mainwindow.h" // Avoid including full mainwindow.h if possible, use forward declared pointer
                               // However, if Orchestrator needs to call specific public methods on MainWindow
                               // beyond emitting signals, it might be needed. For Phase 6 conceptual, try to avoid.

#include <QtConcurrent/QtConcurrent> // For QtConcurrent::run if orchestrator makes its own LLM calls
#include <QDebug> // For placeholder logging
#include <QTimer> // For QTimer::singleShot in nextStep stub

// Placeholder for actual MainWindow if specific methods were needed beyond signals
// For now, we assume communication to MainWindow is via signals.

OrchestratorManager::OrchestratorManager(MainWindow* mainWindow, 
                                         LLMService* llmServiceForOrchestrator, 
                                         CommandLib::CommandManager* commandManager, 
                                         QObject *parent)
    : QObject(parent),
      m_mainWindow(mainWindow),
      m_llmService(llmServiceForOrchestrator),
      m_commandManager(commandManager),
      m_currentTaskIndex(-1),
      m_state(OrchestrationState::Idle) {

    // Connect the internal watcher for LLM tasks initiated by the orchestrator
    connect(&m_llmTaskWatcher, &QFutureWatcher<QVariantMap>::finished,
            this, &OrchestratorManager::handleLlmFutureFinished);

    // If the orchestrator's LLMService emits signals (like responseReady), connect them here.
    // This assumes m_llmService is a fully functional service instance.
    // Note: This connection might be problematic if m_llmService is the same as MainWindow's
    // m_currentService and MainWindow changes it. For conceptual phase, proceed with caution.
    // A robust implementation might require OrchestratorManager to have its own dedicated LLMService instance
    // or a more abstract way to request LLM processing.
    if (m_llmService) {
        // This connection is tricky. If m_llmService is MainWindow's m_currentService,
        // then MainWindow's handleServiceResponse would also fire.
        // For this conceptual phase, let's assume OrchestratorManager's LLM calls are
        // handled via its m_llmTaskWatcher and it doesn't directly listen to m_llmService's responseReady.
        // Instead, it will initiate calls using m_llmService->generateResponse() and process via m_llmTaskWatcher.
        // So, removing direct connection:
        // connect(m_llmService, &LLMService::responseReady, this, &OrchestratorManager::processInternalLlmResponse);
    }
    
    qDebug() << "OrchestratorManager initialized.";
    emit orchestrationStatusUpdate("Orchestrator initialized. Ready.");
}

void OrchestratorManager::startOrchestration(const QStringList& todoList) {
    if (m_state == OrchestrationState::Running) {
        emit orchestratorDisplayMessage("Orchestration is already running.", true);
        return;
    }
    if (todoList.isEmpty()) {
        emit orchestratorDisplayMessage("ToDo list is empty. Nothing to orchestrate.", true);
        return;
    }
    if (!m_llmService || !m_llmService->isConfigured()) {
        emit orchestratorDisplayMessage("Orchestrator's LLM service is not available or not configured.", true);
        emit orchestrationFinished(false, "Orchestrator LLM service not configured.");
        return;
    }

    m_todoList = todoList;
    m_currentTaskIndex = 0;
    m_state = OrchestrationState::Running;
    
    qDebug() << "Orchestration started with ToDo list:" << m_todoList;
    emit orchestrationStatusUpdate("Orchestration started. Tasks: " + m_todoList.join("; "));
    emit orchestratorDisplayMessage("Orchestration started with " + QString::number(m_todoList.size()) + " tasks.");

    nextStep();
}

void OrchestratorManager::stopOrchestration() {
    if (m_state == OrchestrationState::Idle) {
        emit orchestratorDisplayMessage("Orchestration is not running.", false);
        return;
    }
    
    qDebug() << "Orchestration stopped by user or completion.";
    // m_llmTaskWatcher.cancel(); // Cancel any ongoing LLM call by the orchestrator
    // Note: QFuture is not directly cancellable this way. If the thread is running, it completes.
    // Cancellation needs to be cooperative within the lambda. For simplicity, not handled here.

    bool wasRunning = (m_state == OrchestrationState::Running || m_state == OrchestrationState::WaitingForUser || m_state == OrchestrationState::WaitingForCommand);
    m_state = OrchestrationState::Idle;
    m_todoList.clear();
    m_currentTaskIndex = -1;

    emit orchestrationStatusUpdate("Orchestration stopped.");
    if (wasRunning) { // Only emit finished if it was actually doing something
         emit orchestrationFinished(false, "Orchestration stopped by user.");
    }
    emit orchestratorDisplayMessage("Orchestration stopped.");
}

OrchestratorManager::OrchestrationState OrchestratorManager::getState() const {
    return m_state;
}

bool OrchestratorManager::isRunning() const {
    // Consider WaitingForUser and WaitingForCommand as "running" states
    return m_state == OrchestrationState::Running || 
           m_state == OrchestrationState::WaitingForUser ||
           m_state == OrchestrationState::WaitingForCommand;
}

bool OrchestratorManager::hasValidLlmService() const {
    // Ensure m_llmService is not null before calling isConfigured to prevent crashes
    return m_llmService != nullptr && m_llmService->isConfigured();
}

void OrchestratorManager::provideUserInput(const QString& input) {
    if (m_state != OrchestrationState::WaitingForUser) {
        emit orchestratorDisplayMessage("Orchestrator is not waiting for user input.", true);
        return;
    }
    qDebug() << "User input received:" << input;
    emit orchestratorDisplayMessage("User input received: " + input);
    // TODO: Process the input (e.g., use it for the current task)
    // For now, just proceed to next step as a placeholder
    m_state = OrchestrationState::Running; // Assume input was processed
    nextStep();
}

void OrchestratorManager::processExternalCommandResult(const QString& commandName, const QString& output, const QString& error) {
    if (m_state != OrchestrationState::WaitingForCommand) {
        emit orchestratorDisplayMessage("Orchestrator is not waiting for command result for: " + commandName, true);
        // Could be a result for a command not initiated by orchestrator, or state mismatch.
        // For now, we'll accept it if any command was suggested.
    }
    qDebug() << "External command result for" << commandName << "- Output:" << output << "Error:" << error;
    emit orchestratorDisplayMessage(QString("Result for command '%1':\nOutput: %2\nError: %3").arg(commandName, output, error));
    
    // TODO: Check if this command was the one orchestrator was waiting for.
    // Update task status based on output/error.
    
    m_state = OrchestrationState::Running; // Assume command was processed
    nextStep();
}


// Slot for this orchestrator's own LLM calls (e.g., for planning, sub-task generation)
void OrchestratorManager::handleLlmFutureFinished() {
    if (m_state != OrchestrationState::Running && m_state != OrchestrationState::WaitingForUser /* could be waiting for user while LLM thinks */) {
        // This might happen if stopOrchestration was called while an LLM call was in flight.
        qDebug() << "Orchestrator LLM future finished but orchestrator not in active state. State:" << static_cast<int>(m_state);
        return;
    }

    QVariantMap result = m_llmTaskWatcher.result();
    QString responseText = result.value("responseText").toString();
    QString errorString = result.value("errorString").toString();

    // Note: requestPayload and fullResponsePayload are also in 'result' if needed for detailed logging.
    // emit orchestratorDisplayMessage("Orchestrator LLM Request: " + result.value("requestPayload").toString());
    // emit orchestratorDisplayMessage("Orchestrator LLM Full Response: " + result.value("fullResponsePayload").toString());


    if (!errorString.isEmpty()) {
        qDebug() << "Orchestrator LLM call error:" << errorString;
        emit orchestratorDisplayMessage("Orchestrator LLM call failed: " + errorString, true);
        // Decide if this is a fatal error for the orchestration
        // stopOrchestration(); // Or attempt retry, or ask user
        emit orchestrationFinished(false, "Orchestrator LLM call failed: " + errorString);
        m_state = OrchestrationState::Idle; // Set idle on error
        return;
    }

    qDebug() << "Orchestrator LLM call response:" << responseText;
    emit orchestratorDisplayMessage("Orchestrator received LLM response for current task.");
    
    interpretLlmResponseForSubtasks(responseText); // This is a stub
}

// This slot was intended for if m_llmService directly signalled.
// Keeping it for now, but it's not connected if using m_llmTaskWatcher primarily.
void OrchestratorManager::processInternalLlmResponse(const QString& responseText, const QString& /*requestPayload*/, 
                                                     const QString& /*fullResponsePayload*/, const QString& errorString) {
    if (m_state != OrchestrationState::Running) return; // Only process if running

    qDebug() << "Orchestrator (via direct signal) LLM response:" << responseText << "Error:" << errorString;
    if (!errorString.isEmpty()) {
        emit orchestratorDisplayMessage("Orchestrator LLM (direct signal) Error: " + errorString, true);
        // stopOrchestration(); // Or some other error handling
        emit orchestrationFinished(false, "Orchestrator LLM (direct signal) Error: " + errorString);
        m_state = OrchestrationState::Idle;
        return;
    }
    interpretLlmResponseForSubtasks(responseText);
}


// --- Conceptual Stubs for Phase 6 ---
void OrchestratorManager::nextStep() {
    if (m_state != OrchestrationState::Running) {
        qDebug() << "nextStep called but orchestrator not running. State:" << static_cast<int>(m_state);
        return;
    }

    if (m_currentTaskIndex >= m_todoList.size()) {
        qDebug() << "All tasks completed.";
        emit orchestratorDisplayMessage("All tasks completed successfully!", false);
        emit orchestrationStatusUpdate("Orchestration finished: All tasks done.");
        emit orchestrationFinished(true, "All tasks completed.");
        m_state = OrchestrationState::Idle;
        return;
    }

    QString currentTask = m_todoList.at(m_currentTaskIndex);
    qDebug() << "Orchestrator: Processing task" << m_currentTaskIndex + 1 << "/" << m_todoList.size() << ":" << currentTask;
    emit orchestratorStatusUpdate(QString("Processing task %1 of %2: %3")
                                  .arg(m_currentTaskIndex + 1)
                                  .arg(m_todoList.size())
                                  .arg(currentTask));
    
    // Conceptual: For Phase 6, we'll just "complete" the task and move to the next one.
    // A real implementation would call generatePromptForTaskLLM, then process its response.
    // For now, we simulate this by asking the user a generic question for each task.
    
    // Placeholder: Ask LLM (or for this stub, ask user) what to do for currentTask
    // generatePromptForTaskLLM(currentTask); 
    // For this conceptual phase, let's just emit needs user input for each task.
    m_state = OrchestrationState::WaitingForUser;
    emit orchestratorNeedsUserInput(QString("What should be done for task: '%1'? (Conceptual step)").arg(currentTask), "Example action for " + currentTask);
    
    // After user provides input via provideUserInput(), it will call nextStep() again.
    // Or, if we want to auto-complete tasks:
    // emit orchestratorDisplayMessage("Conceptual: Task '" + currentTask + "' marked as done by orchestrator stub.", false);
    // m_currentTaskIndex++;
    // QTimer::singleShot(1000, this, &OrchestratorManager::nextStep); // Simulate work and move to next
}

void OrchestratorManager::executeCurrentTask() {
    // This would be called by nextStep if it decides to execute something directly
    // based on its plan, rather than asking the LLM first.
    // For this conceptual phase, this can remain a stub.
    qDebug() << "Orchestrator: executeCurrentTask (stub)";
    emit orchestratorDisplayMessage("Orchestrator: executeCurrentTask (stub called)", false);
    // Example: if a task was "run command X", it would call:
    // emit orchestratorSuggestsLocalCommand("X");
    // m_state = OrchestrationState::WaitingForCommand;
}

void OrchestratorManager::generatePromptForTaskLLM(const QString& taskDescription) {
    // In a real scenario, this prepares a prompt for the orchestrator's LLM
    // to break down the task or suggest actions.
    qDebug() << "Orchestrator: Generating LLM prompt for task:" << taskDescription << "(stub)";
    emit orchestratorDisplayMessage("Orchestrator: Generating LLM prompt for task: " + taskDescription + " (stub)", false);

    if (!m_llmService || !m_llmService->isConfigured()) {
        emit orchestratorDisplayMessage("Orchestrator's LLM service not available/configured to generate task prompt.", true);
        // Potentially stop orchestration or ask user for manual steps
        return;
    }

    // Construct history for the orchestrator's LLM call
    std::vector<Common::ConversationTurn> history;
    history.push_back({"user", QString("Based on the overall ToDo list: [%1], I am currently working on the task: \"%2\". What are the specific sub-steps, local commands (formatted as ~[command:args]~), or questions I should ask the user to accomplish this task? Be concise.")
                                   .arg(m_todoList.join(", "), taskDescription)
                                   .toStdString()});
    
    // This uses QtConcurrent directly, similar to how services do it.
    // The result will be handled by handleLlmFutureFinished.
    QString serviceName = m_llmService->getName();
    QString modelName = m_llmService->getLastSelectedModel();
    if (modelName.isEmpty() && m_llmService->isConfigured()) { // If no model selected, try to get one
        // This is tricky, fetchModels is async. Orchestrator might need a default model.
        // For simplicity, assume a model is selected or service uses a default.
        // Or, OrchestratorManager should have its own specific model configuration.
        // emit orchestratorDisplayMessage("Orchestrator's LLM service has no model selected.", true);
        // return;
    }


    // The following is a direct LLM call. This is where OrchestratorManager uses an LLM.
    // For simplicity, let's assume the LLMService is capable of handling this.
    // We'll use a simplified call that mimics how services work.
    // A real implementation would need robust error handling and state management here.
    
    // This is a simplified conceptual call. A real one would need more setup
    // like the concrete services (payload construction, etc.).
    // For Phase 6, we are primarily setting up the structure.
    // The actual call is stubbed out to prevent complexity here.
    
    // Instead of a real call, let's simulate a response for the stub.
    // QFuture<QVariantMap> future = QtConcurrent::run([... lambda for actual call ...]);
    // m_llmTaskWatcher.setFuture(future);
    
    // Simulate LLM thinking and then call interpretLlmResponseForSubtasks
    QString simulatedLlmResponse = QString("Okay, for task '%1', first ~[command:echo Starting task %1]~, then ask the user 'What is the target file path?'.")
                                    .arg(taskDescription);
    emit orchestratorDisplayMessage("Orchestrator LLM (simulated) response: " + simulatedLlmResponse, false);
    interpretLlmResponseForSubtasks(simulatedLlmResponse); // Call directly for stub
}

void OrchestratorManager::interpretLlmResponseForSubtasks(const QString& llmResponse) {
    // Parses LLM output for sub-tasks, commands like ~[command:args]~, or questions for the user.
    qDebug() << "Orchestrator: Interpreting LLM response:" << llmResponse << "(stub)";
    emit orchestratorDisplayMessage("Orchestrator: Interpreting LLM response (stub): " + llmResponse, false);

    // Example parsing logic (very basic)
    if (llmResponse.contains("~[command:")) {
        QRegularExpression cmdRegex(R"(~\[command:\s*([^\]]+)\]~)");
        QRegularExpressionMatch match = cmdRegex.match(llmResponse);
        if (match.hasMatch()) {
            QString command = match.captured(1);
            emit orchestratorDisplayMessage("Orchestrator suggests command: " + command, false);
            emit orchestratorSuggestsLocalCommand(command);
            m_state = OrchestrationState::WaitingForCommand;
            return; // Wait for command result
        }
    }
    if (llmResponse.contains("ask the user")) {
         // Extract question part if possible, for now, use a generic one
        QString question = "LLM needs clarification. Response: " + llmResponse;
        int askUserPos = llmResponse.indexOf("ask the user");
        if (askUserPos != -1) {
            int qStart = llmResponse.indexOf('\'', askUserPos);
            if (qStart != -1) {
                int qEnd = llmResponse.indexOf('\'', qStart + 1);
                if (qEnd != -1) {
                    question = llmResponse.mid(qStart + 1, qEnd - qStart - 1);
                }
            }
        }
        emit orchestratorDisplayMessage("Orchestrator needs user input: " + question, false);
        emit orchestratorNeedsUserInput(question);
        m_state = OrchestrationState::WaitingForUser;
        return; // Wait for user input
    }

    // If no specific action parsed, assume task is "done" by LLM's plan and move to next actual task.
    // This is part of the conceptual stub behavior.
    emit orchestratorDisplayMessage("LLM response processed. Moving to next task in ToDo list.", false);
    m_currentTaskIndex++;
    nextStep();
}
```
