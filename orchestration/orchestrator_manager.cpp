#include "orchestration/orchestrator_manager.h" // Adjusted
#include "llm_services/llm_service.h"         // Adjusted
#include "command_lib/command_library.h"    // Adjusted
#include "ui/mainwindow.h"                  // For ConversationTurn & getCurrentSystemPrompt

#include <QDebug>
#include <QTimer>
#include <QCoreApplication> // For invokeMethod arguments

// Define ConversationTurn if it's not globally accessible via mainwindow.h include in llm_service.h
// For this structure, assume mainwindow.h (which defines ConversationTurn) is included by orchestrator_manager.h indirectly or directly

OrchestratorManager::OrchestratorManager(MainWindow* mainWindow, LLMService* taskLlmService, CommandLibrary::CommandManager* commandManager, QObject* parent)
    : QObject(parent), m_mainWindow(mainWindow), m_taskLlmService(taskLlmService), m_commandManager(commandManager),
      m_currentStepIndex(-1), m_isRunning(false), m_isPaused(false), m_waitingForCommandConfirmation(false) {
    // Ensure m_mainWindow is valid
    Q_ASSERT(m_mainWindow);
}

void OrchestratorManager::setTaskLlmService(LLMService* service) {
    m_taskLlmService = service;
}

bool OrchestratorManager::startOrchestration(const QStringList& todoList) {
    if (m_isRunning) {
        emit orchestrationStatusUpdate("Orchestration already in progress.");
        return false;
    }
    if (!m_taskLlmService || !m_taskLlmService->isConfigured()) {
        emit orchestrationStatusUpdate("Task LLM Service not available or not configured.");
        return false;
    }
    if (todoList.isEmpty()) {
        emit orchestrationStatusUpdate("To-Do list is empty.");
        return false;
    }

    m_todoList = todoList;
    m_currentStepIndex = -1;
    m_isRunning = true;
    m_isPaused = false;
    m_lastCommandResult.clear();
    m_lastLlmResponse.clear();

    emit orchestratorDisplayMessage("Orchestrator", "Starting orchestration for: " + m_todoList.join("; "));
    emit orchestrationStatusUpdate("Orchestration started...");
    QTimer::singleShot(100, this, &OrchestratorManager::nextStep);
    return true;
}

void OrchestratorManager::stopOrchestration() {
    if (!m_isRunning) return;
    m_isRunning = false;
    m_isPaused = false;
    emit orchestratorDisplayMessage("Orchestrator", "Orchestration stopped by user.");
    emit orchestrationStatusUpdate("Orchestration stopped.");
    emit orchestrationFinished(false, "Stopped by user.");
}

void OrchestratorManager::nextStep() {
    if (!m_isRunning || m_isPaused || m_waitingForCommandConfirmation) return;

    m_currentStepIndex++;
    m_lastCommandResult.clear();
    m_lastLlmResponse.clear();
    m_currentStepConversationHistory.clear();

    if (m_currentStepIndex >= m_todoList.size()) {
        emit orchestratorDisplayMessage("Orchestrator", "All To-Do items completed!");
        emit orchestrationStatusUpdate("Orchestration finished successfully.");
        emit orchestrationFinished(true, "All tasks completed.");
        m_isRunning = false;
        return;
    }

    QString currentTodo = m_todoList.at(m_currentStepIndex);
    emit orchestratorDisplayMessage("Orchestrator", QString("Processing To-Do #%1: %2").arg(m_currentStepIndex + 1).arg(currentTodo));
    emit orchestrationStatusUpdate(QString("Step %1/%2: %3").arg(m_currentStepIndex + 1).arg(m_todoList.size()).arg(currentTodo));
    executeCurrentStep();
}

void OrchestratorManager::executeCurrentStep() {
    if (!m_isRunning || m_isPaused || !m_taskLlmService || m_currentStepIndex < 0 || m_currentStepIndex >= m_todoList.size()) {
        return;
    }

    QString currentTodoItem = m_todoList.at(m_currentStepIndex);
    QString previousContext;
    if (!m_lastLlmResponse.isEmpty()) previousContext += "Last LLM context: " + m_lastLlmResponse.left(200) + "...\n"; // Truncate for prompt
    if (!m_lastCommandResult.isEmpty()) previousContext += "Last local command result: " + m_lastCommandResult.left(200) + "...\n";

    QString promptForTaskLLM = generatePromptForTaskLLM(currentTodoItem, previousContext);
    emit orchestratorDisplayMessage("Orchestrator (Thought)", "Task LLM Prompt: " + promptForTaskLLM);

    m_currentStepConversationHistory.push_back({"user", promptForTaskLLM.toStdString()});

    QString systemPrompt = m_mainWindow->getCurrentSystemPrompt(); // From MainWindow
    QString selectedModel = m_taskLlmService->getLastSelectedModel();
    if (selectedModel.isEmpty()) { // Safety check, should be set by UI
        emit orchestratorDisplayMessage("Orchestrator (Error)", "No model selected for Task LLM.");
        stopOrchestration(); return;
    }


    emit orchestratorDisplayMessage("Orchestrator", "Sending to Task LLM (" + m_taskLlmService->getName() + " - " + selectedModel + ")");
    m_taskLlmService->generateResponse(
        m_currentStepConversationHistory,
        systemPrompt,
        selectedModel,
        [this](const QString& responseText, const QString& reqPayload, const QString& fullRespPayload, const QString& error) {
            // Use QMetaObject::invokeMethod to ensure slot is called on the OrchestratorManager's thread (main thread if created there)
            QMetaObject::invokeMethod(this, "processTaskLlmResponse", Qt::QueuedConnection,
                                      Q_ARG(QString, responseText),
                                      Q_ARG(QString, reqPayload),
                                      Q_ARG(QString, fullRespPayload),
                                      Q_ARG(QString, error));
        }
    );
}

void OrchestratorManager::processTaskLlmResponse(const QString& responseText, const QString& requestPayload, const QString& fullResponsePayload, const QString& error) {
    if (!m_isRunning && !m_waitingForCommandConfirmation) return; // Check if stopped while waiting

    // Let MainWindow handle logging and supervision console for this LLM interaction
    if (m_mainWindow) {
        m_mainWindow->updateSupervisionAndLogsForLLM(
            m_taskLlmService ? m_taskLlmService->getName() : "Unknown Service",
            m_taskLlmService ? m_taskLlmService->getLastSelectedModel() : "Unknown Model",
            requestPayload, fullResponsePayload, responseText, error
        );
    }


    if (!error.isEmpty()) {
        emit orchestratorDisplayMessage("Orchestrator (Error)", "Task LLM failed: " + error);
        emit orchestrationStatusUpdate("Error from Task LLM. Orchestration paused.");
        emit orchestratorNeedsUserInput("Task LLM failed: " + error + "\nHow to proceed? (Stopping for now)");
        stopOrchestration(); // Stop on LLM error for simplicity, could pause
        return;
    }

    emit orchestratorDisplayMessage("Task LLM ("+(m_taskLlmService ? m_taskLlmService->getName() : "LLM")+")", responseText);
    m_lastLlmResponse = responseText;
    m_currentStepConversationHistory.push_back({"model", responseText.toStdString()});

    QString suggestedCommand;
    QString nextInstructionForUser;
    bool needsCommand = interpretTaskLLMResponse(responseText, suggestedCommand, nextInstructionForUser);

    if (needsCommand && !suggestedCommand.isEmpty()) {
        emit orchestratorDisplayMessage("Orchestrator (Thought)", "Task LLM suggested command: " + suggestedCommand);
        m_waitingForCommandConfirmation = true;
        emit orchestratorSuggestsLocalCommand(suggestedCommand, "Orchestrator wants to execute to achieve: " + m_todoList.at(m_currentStepIndex));
    } else {
        if (isStepCompleted(m_todoList.at(m_currentStepIndex), responseText, "")) {
             emit orchestratorDisplayMessage("Orchestrator", "Step seems complete based on LLM response.");
            QTimer::singleShot(1000, this, &OrchestratorManager::nextStep); // Delay before next
        } else {
            emit orchestratorDisplayMessage("Orchestrator (Thought)", "No command, step not clearly complete. Trying to refine with Task LLM for current step.");
            if (m_currentStepConversationHistory.size() < 4) { // Limit retries on current step
                QTimer::singleShot(1000, this, &OrchestratorManager::executeCurrentStep);
            } else {
                emit orchestratorDisplayMessage("Orchestrator (Warning)", "Max retries for current step reached. Moving to next To-Do item.");
                emit orchestrationStatusUpdate("Stuck on: " + m_todoList.at(m_currentStepIndex) + ". Moving on.");
                QTimer::singleShot(1000, this, &OrchestratorManager::nextStep);
            }
        }
    }
}

void OrchestratorManager::processLocalCommandResult(const QString& commandName, const QString& args, const QString& result, bool success) {
    if (!m_isRunning || !m_waitingForCommandConfirmation) return; // Should only be called when waiting
    m_waitingForCommandConfirmation = false;

    emit orchestratorDisplayMessage("Local Command (" + commandName + ")", "Args: " + args + "\nResult: " + result);
    m_lastCommandResult = result;

    if (!success) {
        emit orchestratorDisplayMessage("Orchestrator (Error)", "Local command execution failed: " + commandName);
        emit orchestrationStatusUpdate("Local command failed. Orchestration paused.");
        emit orchestratorNeedsUserInput("Local command '" + commandName + "' failed. Result: " + result.left(200) + "...\nHow to proceed? (Stopping for now)");
        stopOrchestration(); // Stop on command error for simplicity
        return;
    }

    // Add command result to conversation for Task LLM context
    m_currentStepConversationHistory.push_back({"user",
        QString("Context: The local command '%1 %2' was executed. Its result was: '%3'.\nNow, considering this, please continue with the original task: '%4'")
        .arg(commandName, args, result.left(300) + (result.length() > 300 ? "..." : ""), m_todoList.at(m_currentStepIndex)) // Truncate long results
        .toStdString()});


    if (isStepCompleted(m_todoList.at(m_currentStepIndex), m_lastLlmResponse, result)) {
        emit orchestratorDisplayMessage("Orchestrator", "Step completed after command execution.");
        QTimer::singleShot(1000, this, &OrchestratorManager::nextStep);
    } else {
        emit orchestratorDisplayMessage("Orchestrator (Thought)", "Command executed. Re-querying Task LLM for current step based on result.");
        QTimer::singleShot(1000, this, &OrchestratorManager::executeCurrentStep); // Re-query with new context
    }
}

QString OrchestratorManager::generatePromptForTaskLLM(const QString& currentTodoItem, const QString& previousContext) {
    QString prompt = QString("You are a helpful assistant. Your current high-level sub-task is: \"%1\".\n").arg(currentTodoItem);
    if (!previousContext.isEmpty()) {
        prompt += "Consider the following context from previous interactions for this sub-task:\n" + previousContext + "\n";
    }
    prompt += "Please provide the necessary information, code, or steps to achieve this sub-task. If you determine a local system command needs to be run (like compiling code, listing files, creating a directory), "
              "you MUST explicitly state the command in the format 'COMMAND: full_command_with_arguments'. "
              "For example: 'COMMAND: g++ main.cpp -o myapp'. Do not ask for confirmation to run the command, just state it if needed. "
              "If the sub-task is purely informational or content generation, provide that directly. "
              "Indicate completion if the sub-task is fully addressed by your response.";

    // Simple simulated logic based on keywords - a real orchestrator LLM would be much smarter
    if (currentTodoItem.contains("create a C++ file", Qt::CaseInsensitive) && currentTodoItem.contains("named")) {
        QRegularExpression re("named\\s*['\"]?([^'\"\\s]+)['\"]?");
        QRegularExpressionMatch match = re.match(currentTodoItem);
        QString filename = match.hasMatch() ? match.captured(1) : "example.cpp";
        prompt = QString("Your sub-task is: \"%1\". Please generate the C++ code for a simple 'Hello World' program. After providing the code, if you want it saved, suggest the command to save it using this format: 'COMMAND: save_code %2 content_of_the_code_block_here' (though 'save_code' is not a real local command in this system, the orchestrator would handle it or ask for a real command like 'echo \"...\" > %2'). For now, just provide the code and then state 'Task completed successfully for: create a C++ file named %2' if you think you've done it.").arg(currentTodoItem, filename);
    } else if (currentTodoItem.contains("compile", Qt::CaseInsensitive) && currentTodoItem.contains("named")) {
         QRegularExpression re_source("compile\\s*['\"]?([^'\"\\s]+\\.cpp)['\"]?"); // C++ file
         QRegularExpression re_output("named\\s*['\"]?([^'\"\\s]+)['\"]?");     // Output name
         QRegularExpressionMatch match_source = re_source.match(currentTodoItem);
         QRegularExpressionMatch match_output = re_output.match(currentTodoItem);
         QString source_file = match_source.hasMatch() ? match_source.captured(1) : "source.cpp";
         QString output_file = match_output.hasMatch() ? match_output.captured(1) : "a.out";
        prompt = QString("Your sub-task is: \"%1\". Provide the exact g++ command to compile '%2' into an executable named '%3'. Use the format 'COMMAND: g++ your_args_here'.").arg(currentTodoItem, source_file, output_file);
    } else if (currentTodoItem.contains("list files", Qt::CaseInsensitive)) {
        prompt = QString("Your sub-task is: \"%1\". Provide the command to list files in the current directory. Use the format 'COMMAND: your_command_here'.").arg(currentTodoItem);
    }
    return prompt;
}

bool OrchestratorManager::interpretTaskLLMResponse(const QString& response, QString& outSuggestedCommand, QString& outNextInstructionForUser) {
    QString commandPrefix = "COMMAND:";
    int cmdIdx = response.indexOf(commandPrefix, 0, Qt::CaseInsensitive);
    if (cmdIdx != -1) {
        outSuggestedCommand = response.mid(cmdIdx + commandPrefix.length()).trimmed();
        int newlineIdx = outSuggestedCommand.indexOf('\n');
        if (newlineIdx != -1) {
            outSuggestedCommand = outSuggestedCommand.left(newlineIdx).trimmed();
        }
        // Further sanitization: remove potential explanations after command
        if (outSuggestedCommand.contains("This command will", Qt::CaseInsensitive)) {
            outSuggestedCommand = outSuggestedCommand.left(outSuggestedCommand.indexOf("This command will", 0, Qt::CaseInsensitive)).trimmed();
        }
        if (!outSuggestedCommand.isEmpty()) return true;
    }
    outSuggestedCommand.clear(); // Ensure it's empty if no valid command found
    return false;
}

bool OrchestratorManager::isStepCompleted(const QString& todoItem, const QString& llmResponse, const QString& commandResult) {
    // More nuanced completion check
    if (llmResponse.contains("Task completed successfully for:", Qt::CaseInsensitive) &&
        llmResponse.toLower().contains(todoItem.left(20).toLower(), Qt::CaseInsensitive)) { // Check if LLM confirms for this specific task
        return true;
    }
    if (!commandResult.isEmpty()) { // If a command was run
        if (commandResult.startsWith("Error:", Qt::CaseInsensitive)) return false; // Command failed

        if (todoItem.contains("compile", Qt::CaseInsensitive) && (commandResult.contains("exit code 0") || commandResult.contains("uccessfully"))) {
            return true;
        }
        if (todoItem.contains("create a directory", Qt::CaseInsensitive) && (commandResult.contains("exit code 0") || commandResult.contains("uccessfully") || commandResult.contains("Directory created"))) {
            return true;
        }
         if (todoItem.contains("list files", Qt::CaseInsensitive) && !commandResult.startsWith("Error")) { // Assume successful listing
            return true;
        }
        // Add more specific checks based on expected command outputs for different todoItem types
    }

    // If the task was to generate text, and the LLM provided a substantial response without errors or commands
    if ( (todoItem.contains("write", Qt::CaseInsensitive) ||
          todoItem.contains("generate", Qt::CaseInsensitive) ||
          todoItem.contains("explain", Qt::CaseInsensitive) ||
          todoItem.contains("create a C++ file", Qt::CaseInsensitive) // If LLM provides code block
         ) &&
         commandResult.isEmpty() && // No command was expected or run for this completion path
         !llmResponse.isEmpty() && llmResponse.length() > 50 && // Arbitrary length check for "substance"
         !llmResponse.startsWith("Error", Qt::CaseInsensitive) &&
         !interpretTaskLLMResponse(llmResponse, QString(), QString()) ) { // And it's not trying to suggest a command now
        qDebug() << "Assuming generation task complete based on LLM response length and no command suggestion.";
        return true;
    }


    qDebug() << "Step not deemed complete. Todo:" << todoItem << "LLM:" << llmResponse.left(50) << "Cmd:" << commandResult.left(50);
    return false;
}