// mainwindow.cpp

#include "mainwindow.h" // Should be ui/mainwindow.h if main.cpp is in root

// Qt Includes
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QWidget>
#include <QTextEdit>       // For chatDisplay, promptInput
#include <QPlainTextEdit>  // For supervisionConsole
#include <QPushButton>
#include <QComboBox>
#include <QSplitter>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QActionGroup>    // For serviceMenu
#include <QFileDialog>
#include <QMessageBox>
#include <QInputDialog>
#include <QStatusBar>
#include <QSettings>
#include <QApplication>    // For qApp
#include <QDir>
#include <QTimer>          // For QTimer::singleShot
#include <QScrollBar>      // For auto-scrolling text edits
#include <QFontDatabase>   // For monospaced font
#include <QClipboard>      // Example: If you add copy functionality
#include <QProcess>        // If directly interacting with QProcess here
#include <QStandardPaths>  // For log file paths

// Qt JSON (if you decide to use it for history, but nlohmann for LLM interaction)
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

// Your LLM Service Headers (assuming they are in llm_services/ subdirectory)
#include "llm_services/llm_service.h"
#include "llm_services/gemini_service.h"
#include "llm_services/ollama_service.h"
#include "llm_services/openai_compatible_service.h"

// Other custom class headers (adjust paths as needed)
// #include "orchestration/orchestrator_manager.h"
// #include "terminal_interaction/terminal_process_handler.h"

// Third-party includes (already in service files, but good to be aware)
// #include "vendor/httplib.h"
#include "vendor/nlohmann/json.hpp" // For JSON history if preferred

using json = nlohmann::json; // Alias for nlohmann::json

// ns_turn helpers for nlohmann::json and ConversationTurn
// Define these here or in a common utility header if used across multiple .cpp files
// that don't include the service .cpp files directly.
// For now, assuming service .cpp files have their own or this is sufficient.
namespace ns_turn {
    void to_json(json& j, const ConversationTurn& t) {
        j = json{ {"role", t.role}, {"text", t.text} };
    }
    void from_json(const json& j, ConversationTurn& t) {
        j.at("role").get_to(t.role);
        j.at("text").get_to(t.text);
    }
}

const QString config_path = "universal_llm_client_settings.ini"; // For QSettings
const QString command_config_path = "commands_config.json";      // For CommandManager


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      commandManager(command_config_path) // Initialize CommandManager with its config path
{
    setupUi();    // Creates UI elements
    setupMenus(); // Creates menu bar and actions

    if (!sessionLogger.startNewSessionLog("MainSession")) {
        qWarning() << "Failed to start initial log session!";
    }
    statusBar->showMessage(QString("Logging to: %1").arg(sessionLogger.getCurrentLogFilePath()), 7000);

    loadAppSettings(); // Loads last service, system prompt, etc.

    // Initialize with a default service or the last used one
    if (m_lastSelectedServiceType == "Ollama") {
        selectServiceOllama();
    } else if (m_lastSelectedServiceType == "LM Studio") { // Match exact name used in getName()
        selectServiceLMStudio();
    } else if (m_lastSelectedServiceType == "VLLM") { // Match exact name used in getName()
        selectServiceVLLM();
    } else { // Default to Gemini
        selectServiceGemini(); // This will also call fetchModels
    }

    if (systemPromptText.isEmpty()){
        statusBar->showMessage("Tip: Set a System Prompt under Settings for customized AI behavior.", 5000);
    } else {
        // This might be overwritten by model loading status, so maybe delay or make status bar more complex
    }
    setWindowTitle("Universal LLM Client");

    // Example initialization for other managers if they were members
    // orchestratorManager = std::make_unique<OrchestratorManager>(this, &commandManager, &sessionLogger);
    // connect(orchestratorManager.get(), &OrchestratorManager::orchestrationStateChanged, this, &MainWindow::onOrchestratorStateChanged);
    // connect(orchestratorManager.get(), &OrchestratorManager::appendToSupervision, this, &MainWindow::appendToSupervisionConsole);

    // terminalHandler = std::make_unique<TerminalProcessHandler>(this);
    // connect(terminalHandler.get(), &TerminalProcessHandler::outputReady, this, &MainWindow::handleTerminalOutput);
    // connect(terminalHandler.get(), &TerminalProcessHandler::errorOutputReady, this, &MainWindow::handleTerminalOutput); // Or separate slot
}

MainWindow::~MainWindow() {
    saveAppSettings(); // Save on exit
    // sessionLogger's destructor handles closing the log file
    // unique_ptrs for services, orchestrator, terminal handler clean up automatically
}

void MainWindow::setupUi() {
    QWidget *centralWidget = new QWidget(this);
    QHBoxLayout *topLayout = new QHBoxLayout(centralWidget);

    mainSplitter = new QSplitter(Qt::Horizontal, this);

    // --- Left Pane (Chat Area) ---
    QWidget *leftPaneWidget = new QWidget(mainSplitter);
    QVBoxLayout *leftLayout = new QVBoxLayout(leftPaneWidget);

    modelSelector = new QComboBox(leftPaneWidget);
    modelSelector->setToolTip("Select the model for the current LLM service.");
    connect(modelSelector, &QComboBox::currentTextChanged, this, &MainWindow::modelSelectionChanged);

    chatDisplay = new QTextEdit(leftPaneWidget);
    chatDisplay->setReadOnly(true);
    chatDisplay->setAcceptRichText(true); // To allow HTML formatting

    promptInput = new QTextEdit(leftPaneWidget);
    promptInput->setFixedHeight(100); // Good starting height for multi-line
    promptInput->setPlaceholderText("Enter your prompt here (Ctrl+Enter to send)...");
    promptInput->installEventFilter(this); // For Ctrl+Enter

    sendButton = new QPushButton("Send", leftPaneWidget); // Simpler text
    sendButton->setToolTip("Send prompt (Ctrl+Enter)");
    connect(sendButton, &QPushButton::clicked, this, &MainWindow::onSendClicked);

    QHBoxLayout *inputControlsLayout = new QHBoxLayout();
    inputControlsLayout->addWidget(new QLabel("Model:", leftPaneWidget));
    inputControlsLayout->addWidget(modelSelector, 1); // Give combobox more space

    leftLayout->addLayout(inputControlsLayout);
    leftLayout->addWidget(chatDisplay, 1); // chatDisplay takes up most vertical space

    QHBoxLayout *promptAreaLayout = new QHBoxLayout(); // Renamed for clarity
    promptAreaLayout->addWidget(promptInput, 1);
    promptAreaLayout->addWidget(sendButton);
    leftLayout->addLayout(promptAreaLayout);

    leftPaneWidget->setLayout(leftLayout);
    mainSplitter->addWidget(leftPaneWidget);

    // --- Right Pane (Supervision Console) ---
    supervisionConsole = new QPlainTextEdit(mainSplitter);
    supervisionConsole->setReadOnly(true);
    supervisionConsole->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    supervisionConsole->setLineWrapMode(QPlainTextEdit::NoWrap); // Good for code/JSON
    supervisionConsole->setPlaceholderText("LLM requests, responses, and local command execution will appear here...");

    mainSplitter->addWidget(supervisionConsole);
    mainSplitter->setStretchFactor(0, 2); // Chat area takes 2/3 width
    mainSplitter->setStretchFactor(1, 1); // Supervision takes 1/3 width
    mainSplitter->setSizes({600, 300});   // Initial rough sizes

    topLayout->addWidget(mainSplitter);
    setCentralWidget(centralWidget);

    statusBar = new QStatusBar(this);
    setStatusBar(statusBar);
    statusBar->showMessage("Ready.");

    resize(1000, 750); // Adjusted initial size
}

bool MainWindow::eventFilter(QObject *obj, QEvent *event) {
    if (obj == promptInput) {
        if (event->type() == QEvent::KeyPress) {
            QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);
            if ((keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) &&
                (keyEvent->modifiers() & Qt::ControlModifier)) {
                onSendClicked();
                return true; // Event handled
            }
        }
    }
    return QMainWindow::eventFilter(obj, event);
}

void MainWindow::setupMenus() {
    // File Menu
    QMenu *fileMenu = menuBar()->addMenu(tr("&File"));
    newAction = new QAction(tr("&New Chat"), this);
    connect(newAction, &QAction::triggered, this, [this](){ newChat(true); }); // Use lambda for default arg
    fileMenu->addAction(newAction);

    loadHistoryAction = new QAction(tr("&Load History..."), this);
    connect(loadHistoryAction, &QAction::triggered, this, &MainWindow::loadHistory);
    fileMenu->addAction(loadHistoryAction);

    saveHistoryAction = new QAction(tr("&Save History As..."), this);
    connect(saveHistoryAction, &QAction::triggered, this, &MainWindow::saveHistory);
    fileMenu->addAction(saveHistoryAction);

    saveLastResponseAction = new QAction(tr("Save &Last AI Response As..."), this);
    connect(saveLastResponseAction, &QAction::triggered, this, &MainWindow::saveLastResponse);
    fileMenu->addAction(saveLastResponseAction);
    fileMenu->addSeparator();
    exitAction = new QAction(tr("E&xit"), this);
    connect(exitAction, &QAction::triggered, qApp, &QApplication::quit);
    fileMenu->addAction(exitAction);

    // LLM Service Menu
    serviceMenu = menuBar()->addMenu(tr("&Service"));
    QActionGroup* serviceGroup = new QActionGroup(this); // Keep as local variable
    serviceGroup->setExclusive(true);

    auto addServiceAction = [&](const QString& name, void (MainWindow::*slot)()) {
        QAction* action = serviceGroup->addAction(name);
        action->setCheckable(true);
        connect(action, &QAction::triggered, this, slot);
        serviceMenu->addAction(action);
        return action; // In case you need to store it, though not strictly needed here
    };

    addServiceAction(tr("Google &Gemini"), &MainWindow::selectServiceGemini);
    addServiceAction(tr("&Ollama"), &MainWindow::selectServiceOllama);
    addServiceAction(tr("&LM Studio"), &MainWindow::selectServiceLMStudio);
    addServiceAction(tr("&VLLM (OpenAI API)"), &MainWindow::selectServiceVLLM);

    serviceMenu->addSeparator();
    configureServiceAction = new QAction(tr("&Configure Current Service..."), this);
    connect(configureServiceAction, &QAction::triggered, this, &MainWindow::configureCurrentService);
    serviceMenu->addAction(configureServiceAction);

    // Settings Menu
    QMenu *settingsMenu = menuBar()->addMenu(tr("&Settings"));
    setSystemPromptAction = new QAction(tr("Set System &Prompt..."), this);
    connect(setSystemPromptAction, &QAction::triggered, this, &MainWindow::setSystemPrompt);
    settingsMenu->addAction(setSystemPromptAction);

    // Example: Tools Menu (if you add command management UI later)
    // QMenu *toolsMenu = menuBar()->addMenu(tr("&Tools"));
    // QAction *manageCmdsAction = new QAction(tr("Manage Local Commands..."), this);
    // connect(manageCmdsAction, &QAction::triggered, this, [this](){
    //     // Open a dialog to manage commandManager (enable/disable, add OS wrappers)
    //     // This would involve creating a new QDialog class.
    //     QMessageBox::information(this, "TODO", "Command management UI not yet implemented.");
    // });
    // toolsMenu->addAction(manageCmdsAction);
}

void MainWindow::loadAppSettings() {
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, "MyCompany", "UniversalLLMClientApp"); // More specific
    systemPromptText = settings.value("systemPrompt", "").toString();
    m_lastSelectedServiceType = settings.value("lastServiceType", "Google Gemini").toString(); // Default to Gemini
    // Individual services load their own settings (API keys, URLs) in their constructors
}

void MainWindow::saveAppSettings() {
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, "MyCompany", "UniversalLLMClientApp");
    settings.setValue("systemPrompt", systemPromptText);
    if (currentService) {
        settings.setValue("lastServiceType", currentService->getName());
    }
    // Individual services save their settings in their `configureService` or destructors if needed
}

void MainWindow::setActiveService(std::unique_ptr<LLMService> service) {
    currentService = std::move(service);
    modelSelector->clear(); // Clear previous models
    sendButton->setEnabled(false); // Disable send until models are loaded/selected

    if (currentService) {
        setWindowTitle(QString("Universal LLM Client - %1").arg(currentService->getName()));
        statusBar->showMessage(QString("Switched to %1. Fetching models...").arg(currentService->getName()));

        // Update checked state in serviceMenu
        for(QAction* action : serviceMenu->actions()){
            if(action->isCheckable()){
                // A bit fragile, relies on action text containing the service name start
                bool isCurrent = action->text().contains(currentService->getName().split(" ").first(), Qt::CaseInsensitive);
                if (action->isChecked() != isCurrent) { // Avoid unnecessary signals
                    action->setChecked(isCurrent);
                }
            }
        }

        if (!currentService->isConfigured()) {
            QString serviceName = currentService->getName(); // Cache before potential move/reset
            if (QMessageBox::Yes == QMessageBox::question(this, "Service Not Configured",
                                                           QString("%1 is not configured. Configure it now?").arg(serviceName),
                                                           QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes)) {
                if (!currentService->configureService(this)) { // configureService might reset currentService on failure
                    statusBar->showMessage(QString("Configuration for %1 cancelled or failed.").arg(serviceName), 3000);
                    // currentService.reset(); // Optionally clear if config fails critically
                    // setActiveService(nullptr); // Or select a default
                    return; // Don't fetch models if config failed
                }
                // If configureService was successful, currentService should still be valid
            } else {
                statusBar->showMessage(QString("%1 not configured. Please configure via Service menu.").arg(serviceName), 3000);
                // currentService.reset();
                return; // Don't fetch models
            }
        }
        // If service is now configured (either initially or after prompt)
        if (currentService && currentService->isConfigured()) {
             fetchModelsForCurrentService();
        } else if (currentService) { // Still not configured after prompt
            modelSelector->addItem(QString("%1 not configured").arg(currentService->getName()));
            modelSelector->setEnabled(false);
        }


    } else { // No service selected
        setWindowTitle("Universal LLM Client");
        statusBar->showMessage("No LLM service selected.");
        modelSelector->addItem("No service selected");
        modelSelector->setEnabled(false);
    }
}

void MainWindow::fetchModelsForCurrentService() {
    if (!currentService || !currentService->isConfigured()) {
        modelSelector->clear();
        modelSelector->addItem(currentService ? QString("%1 not configured").arg(currentService->getName()) : "No service");
        modelSelector->setEnabled(false);
        sendButton->setEnabled(false);
        return;
    }

    modelSelector->clear();
    modelSelector->addItem("Loading models...");
    modelSelector->setEnabled(false);
    sendButton->setEnabled(false); // Keep disabled while loading

    currentService->fetchModels([this](QStringList models, QString error) {
        // Use QMetaObject::invokeMethod to ensure GUI updates happen on the main thread
        QMetaObject::invokeMethod(this, [this, models, error]() {
            modelSelector->clear(); // Clear "Loading models..."
            if (!currentService) return; // Service might have changed while fetching

            if (!error.isEmpty()) {
                modelSelector->addItem(QString("Error: %1").arg(error.left(50))); // Truncate long errors
                statusBar->showMessage(QString("Failed to fetch models for %1: %2").arg(currentService->getName(), error), 5000);
                sessionLogger.logError(QString("FetchModels Error (%1): %2").arg(currentService->getName()).arg(error));
                sendButton->setEnabled(false);
            } else if (models.isEmpty()) {
                modelSelector->addItem("No models available");
                statusBar->showMessage(QString("No models found for %1.").arg(currentService->getName()), 3000);
                sendButton->setEnabled(false);
            } else {
                modelSelector->addItems(models);
                modelSelector->setEnabled(true);
                // sendButton will be enabled once a model is actually selected by modelSelectionChanged

                QString lastModel = currentService->getLastSelectedModel();
                int idx = models.indexOf(lastModel);
                if (idx != -1) {
                    modelSelector->setCurrentIndex(idx);
                } else if (!models.isEmpty()) {
                    modelSelector->setCurrentIndex(0);
                }
                // modelSelectionChanged will be triggered, enabling sendButton if model is valid
                statusBar->showMessage(QString("Models loaded for %1.").arg(currentService->getName()), 3000);
            }
        }, Qt::QueuedConnection);
    });
}

void MainWindow::modelSelectionChanged(const QString& modelName) {
    if (!currentService) return;

    if (!modelName.isEmpty() && !modelName.startsWith("Error:") &&
        !modelName.startsWith("Loading models...") && !modelName.startsWith("No models")) {
        currentService->setLastSelectedModel(modelName);
        // Save this specific model for this service type
        QSettings settings(QSettings::IniFormat, QSettings::UserScope, "MyCompany", "UniversalLLMClientApp");
        QString serviceKeyBase = currentService->getName(); // Assuming getName() is unique enough
        serviceKeyBase.remove(QRegularExpression("[^a-zA-Z0-9_]")); // Sanitize
        settings.setValue(serviceKeyBase + "/lastModel", modelName);

        statusBar->showMessage(QString("Model set to: %1 for %2").arg(modelName, currentService->getName()), 2000);
        sendButton->setEnabled(true); // Enable send button now that a valid model is selected
    } else {
        sendButton->setEnabled(false); // Disable if model selection is invalid
    }
}

void MainWindow::selectServiceGemini() {
    setActiveService(std::make_unique<GeminiService>(this));
}
void MainWindow::selectServiceOllama() {
    setActiveService(std::make_unique<OllamaService>(this));
}
void MainWindow::selectServiceLMStudio() {
    setActiveService(std::make_unique<OpenAICompatibleService>("LM Studio", "http://localhost:1234", this));
}
void MainWindow::selectServiceVLLM() {
    setActiveService(std::make_unique<OpenAICompatibleService>("VLLM", "http://localhost:8000", this));
}

void MainWindow::configureCurrentService() {
    if (currentService) {
        QString serviceName = currentService->getName(); // Cache name
        if (currentService->configureService(this)) { // This might change internal state of service
            statusBar->showMessage(QString("%1 configuration updated. Re-fetching models...").arg(serviceName), 3000);
            // currentService might have been reconfigured, ensure it's still valid
            if (currentService && currentService->isConfigured()){
                 fetchModelsForCurrentService();
            } else if (currentService) { // Still exists but not configured
                modelSelector->clear();
                modelSelector->addItem(QString("%1 not configured").arg(serviceName));
                modelSelector->setEnabled(false);
                sendButton->setEnabled(false);
            }
        } else {
            statusBar->showMessage(QString("Configuration for %1 cancelled or no changes made.").arg(serviceName), 3000);
        }
    } else {
        QMessageBox::information(this, "No Service Selected", "Please select an LLM service from the 'Service' menu first.");
    }
}

void MainWindow::appendToChatDisplay(const QString& role, const QString& text) {
    QString roleDisplay = role;
    QString roleColor = "blue"; // Default for AI

    if (role.compare("You", Qt::CaseInsensitive) == 0 || role.compare("User", Qt::CaseInsensitive) == 0) {
        roleDisplay = "You";
        roleColor = "green";
    } else if (role.compare("System", Qt::CaseInsensitive) == 0 || role.startsWith("Error", Qt::CaseInsensitive)) {
        roleColor = "red";
    }
    // Keep other roles (like service names) as is, with default color

    // Basic HTML for some formatting (bold role, color, line breaks)
    QString formattedText = QString("<div style='margin-bottom: 8px;'>"
                                    "<b style='color:%1;'>%2:</b><br>"
                                    "%3"
                                    "</div>")
                                .arg(roleColor, roleDisplay, text.toHtmlEscaped().replace("\n", "<br>"));
    chatDisplay->moveCursor(QTextCursor::End);
    chatDisplay->insertHtml(formattedText);
    chatDisplay->moveCursor(QTextCursor::End); // Ensure scroll to bottom
}

void MainWindow::appendToSupervisionConsole(const QString& title, const QString& content, bool isJson) {
    supervisionConsole->moveCursor(QTextCursor::End);
    QString timestamp = QTime::currentTime().toString("HH:mm:ss.zzz");
    supervisionConsole->insertPlainText(QString("\n--- %1 (%2) ---\n").arg(title, timestamp));

    if (isJson) {
        QJsonDocument doc = QJsonDocument::fromJson(content.toUtf8());
        if (!doc.isNull()) {
            supervisionConsole->insertPlainText(doc.toJson(QJsonDocument::Indented));
        } else {
            supervisionConsole->insertPlainText(content); // Fallback if not valid JSON
        }
    } else {
        supervisionConsole->insertPlainText(content);
    }
    supervisionConsole->insertPlainText("\n");
    supervisionConsole->verticalScrollBar()->setValue(supervisionConsole->verticalScrollBar()->maximum());
}

void MainWindow::onSendClicked() {
    QString prompt = promptInput->toPlainText().trimmed();
    if (prompt.isEmpty()) {
        statusBar->showMessage("Prompt cannot be empty.", 2000);
        return;
    }

    if (!currentService) {
        QMessageBox::warning(this, "No Service", "Please select an LLM service from the 'Service' menu.");
        return;
    }
    if (!currentService->isConfigured()) {
         QMessageBox::warning(this, "Service Not Configured", QString("%1 is not configured. Please configure it via the 'Service' menu.").arg(currentService->getName()));
        return;
    }

    QString selectedModel = modelSelector->currentText();
    if (selectedModel.isEmpty() || selectedModel.startsWith("Error:") ||
        selectedModel.startsWith("Loading models...") || selectedModel.startsWith("No models")) {
        QMessageBox::warning(this, "No Model Selected", "Please select a valid model for the current service.");
        return;
    }

    sessionLogger.logUserPrompt(prompt);
    appendToChatDisplay("You", prompt);
    conversationHistory.push_back({"user", prompt.toStdString()});
    promptInput->clear(); // Clear after sending

    statusBar->showMessage(QString("Sending to %1 (%2)...").arg(currentService->getName(), selectedModel));
    sendButton->setEnabled(false);
    modelSelector->setEnabled(false);
    promptInput->setEnabled(false); // Disable prompt while waiting


    currentService->generateResponse(
        conversationHistory, systemPromptText, selectedModel,
        [this, selectedModel](QString responseText, QString requestPayload, QString fullResponsePayload, QString error) {
            // This lambda is the callback, executed when the async LLM call completes.
            // Ensure GUI updates are on the main thread (QtConcurrent usually handles this for QFutureWatcher slots,
            // but for direct lambdas, QMetaObject::invokeMethod is safest if the lambda itself wasn't invoked via a signal).
            // Since this lambda is connected to a QFutureWatcher in the service, or called from a slot, it should be fine.

            sendButton->setEnabled(true);
            modelSelector->setEnabled(true);
            promptInput->setEnabled(true); // Re-enable prompt

            QString llmName = currentService ? currentService->getName() : "Unknown LLM";

            appendToSupervisionConsole(QString("LLM Request to %1 (%2)").arg(llmName, selectedModel), requestPayload, true);
            sessionLogger.logLlmRequest(llmName, selectedModel, requestPayload);

            if (!error.isEmpty()) {
                appendToSupervisionConsole(QString("LLM Error from %1").arg(llmName), error, !error.startsWith("Error: HTTP")); // Pretty print if it's likely JSON
                sessionLogger.logLlmResponse(llmName, selectedModel, "", fullResponsePayload.isEmpty() ? error : fullResponsePayload); // Log error
                sessionLogger.logError(QString("LLM Error (%1): %2").arg(llmName, error));

                appendToChatDisplay("Error", QString("LLM Error from %1: %2").arg(llmName, error));
                QMessageBox::critical(this, "LLM Error", error);
                statusBar->showMessage(QString("Error from %1: %2...").arg(llmName, error.left(40)), 5000);
            } else {
                appendToSupervisionConsole(QString("LLM Response from %1 (%2)").arg(llmName, selectedModel), fullResponsePayload, true);
                sessionLogger.logLlmResponse(llmName, selectedModel, responseText, fullResponsePayload);

                conversationHistory.push_back({"model", responseText.toStdString()});
                lastModelResponseText = responseText;
                appendToChatDisplay(llmName, responseText); // Use service name as role
                statusBar->showMessage(QString("Response received from %1.").arg(llmName), 3000);

                processLlmResponseForCommands(responseText); // Check if AI suggested a local command
            }
        });
}

void MainWindow::newChat(bool confirm) {
    bool doNewChat = !confirm; // If confirm is false, do it directly
    if (confirm) {
        if (QMessageBox::Yes == QMessageBox::question(this, "New Chat",
                                                    "Start a new chat session? Unsaved history will be lost.",
                                                    QMessageBox::Yes | QMessageBox::No, QMessageBox::No)) {
            doNewChat = true;
        }
    }

    if (doNewChat) {
        conversationHistory.clear();
        lastModelResponseText.clear();
        chatDisplay->clear(); // Clear the main chat display
        supervisionConsole->clear(); // Clear the supervision console

        if (sessionLogger.startNewSessionLog()) {
             statusBar->showMessage(QString("New chat. Logging to: %1").arg(sessionLogger.getCurrentLogFilePath()), 5000);
        } else {
            statusBar->showMessage("New chat. Error starting new log.", 5000);
        }

        if (!systemPromptText.isEmpty()) {
            appendToSupervisionConsole("System Prompt Active", systemPromptText);
            sessionLogger.logInfo("System Prompt: " + systemPromptText);
        }
    }
}

void MainWindow::setSystemPrompt() {
    bool ok;
    QString newPrompt = QInputDialog::getMultiLineText(this, "Set System Prompt",
                                                       "Enter instructions for the AI (applied at the start of conversations):",
                                                       systemPromptText, &ok);
    if (ok) { // User clicked OK (newPrompt might be empty if they cleared it)
        systemPromptText = newPrompt;
        saveAppSettings(); // Save it persistently
        if (systemPromptText.isEmpty()) {
            statusBar->showMessage("System Prompt cleared.", 3000);
            sessionLogger.logInfo("System Prompt cleared.");
        } else {
            statusBar->showMessage(QString("System Prompt set: \"%1...\"").arg(systemPromptText.left(30)), 3000);
            sessionLogger.logInfo("System Prompt updated: " + systemPromptText);
        }
        if (QMessageBox::Yes == QMessageBox::question(this, "System Prompt Updated",
                                                     "System prompt changed. Start a new chat for it to take full effect?",
                                                     QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes)) {
            newChat(false); // Start new chat without further confirmation
        }
    }
}

void MainWindow::processLlmResponseForCommands(const QString& llmResponseText) {
    QString commandPrefix = "COMMAND:"; // Case-sensitive, or use Qt::CaseInsensitive
    int cmdStartIndex = llmResponseText.indexOf(commandPrefix);

    if (cmdStartIndex != -1) {
        QString fullCommandInstruction = llmResponseText.mid(cmdStartIndex + commandPrefix.length()).trimmed();
        QStringList parts = fullCommandInstruction.split(" ", Qt::SkipEmptyParts); // More robust splitting

        if (!parts.isEmpty()) {
            QString cmdName = parts.first();
            parts.removeFirst(); // Remove command name from args
            QString cmdArgsJoined = parts.join(" ");

            // Check if this command is known AND enabled by the CommandManager
            std::vector<std::string> enabled_commands_std = commandManager.getCommandList(false); // Get enabled commands
            QStringList enabled_commands_qt;
            for(const auto&s : enabled_commands_std) { enabled_commands_qt.append(QString::fromStdString(s)); }

            bool known_and_enabled = false;
            for(const QString& enabled_cmd : enabled_commands_qt) {
                if (enabled_cmd.compare(cmdName, Qt::CaseInsensitive) == 0) {
                    known_and_enabled = true;
                    cmdName = enabled_cmd; // Use the canonical casing from commandManager
                    break;
                }
            }


            if (known_and_enabled) {
                appendToSupervisionConsole(QString("LLM Suggested Local Command"),
                                           QString("Instruction: %1").arg(fullCommandInstruction));
                sessionLogger.logInfo("LLM Suggested Command: " + fullCommandInstruction);

                int userChoice = QMessageBox::question(this, "Execute Local Command?",
                                                       QString("The AI suggests executing the following local command:\n\n%1\n\nDo you want to allow this?").arg(fullCommandInstruction),
                                                       QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel, QMessageBox::No);

                if (userChoice == QMessageBox::Yes) {
                    sessionLogger.logInfo("User approved execution of: " + fullCommandInstruction);
                    std::string result_str = commandManager.executeCommand(fullCommandInstruction.toStdString());
                    QString result = QString::fromStdString(result_str);

                    sessionLogger.logLocalCommand(cmdName, cmdArgsJoined, result);
                    appendToSupervisionConsole(QString("Local Command Executed: %1").arg(cmdName),
                                               QString("Full Instruction: %1\nArgs: %2\nResult:\n%3").arg(fullCommandInstruction, cmdArgsJoined, result));

                    QString feedbackToLlm = QString("SYSTEM_NOTE: Executed local command '%1'. Result: %2").arg(fullCommandInstruction, result.left(500) + (result.length() > 500 ? "..." : ""));
                    appendToChatDisplay("System", feedbackToLlm);
                    conversationHistory.push_back({"user", feedbackToLlm.toStdString()}); // Feed back to LLM as user/system turn
                    // Optionally, trigger another onSendClicked if the workflow demands immediate LLM processing of this result
                } else {
                    QString msg = "User denied or cancelled execution of local command: " + fullCommandInstruction;
                    sessionLogger.logInfo(msg);
                    appendToSupervisionConsole("Local Command Execution Denied/Cancelled", msg);
                    appendToChatDisplay("System", "Local command execution denied or cancelled by user.");
                    // Also inform the LLM that the command was not executed.
                    QString feedbackToLlm = QString("SYSTEM_NOTE: Local command '%1' was NOT executed (denied by user).").arg(fullCommandInstruction);
                    conversationHistory.push_back({"user", feedbackToLlm.toStdString()});
                }
            } else {
                QString msg = "LLM suggested unknown or disabled command: " + fullCommandInstruction;
                 sessionLogger.logInfo(msg);
                 appendToSupervisionConsole("LLM Suggested Invalid Command", msg);
                 appendToChatDisplay("System", msg);
                 QString feedbackToLlm = QString("SYSTEM_NOTE: The command '%1' is not available or not enabled.").arg(fullCommandInstruction);
                 conversationHistory.push_back({"user", feedbackToLlm.toStdString()});

            }
        }
    }
}


// --- History Load/Save using nlohmann::json ---
bool MainWindow::saveHistoryToFile(const QString& filePath) {
    if (conversationHistory.empty()) {
        QMessageBox::information(this, "Save History", "Conversation history is empty. Nothing to save.");
        return true; // Or false, depending on desired behavior for empty history
    }

    json history_json_array = json::array();
    for (const auto& turn : conversationHistory) {
        history_json_array.push_back(ns_turn::ConversationTurn{turn.role, turn.text}); // Uses ns_turn::to_json
    }

    // Optionally, add system prompt and current model/service to the saved history for context
    json full_session_data;
    full_session_data["appVersion"] = QCoreApplication::applicationVersion(); // Example metadata
    full_session_data["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    if (currentService) {
        full_session_data["serviceName"] = currentService->getName().toStdString();
        full_session_data["modelName"] = currentService->getLastSelectedModel().toStdString();
    }
    if (!systemPromptText.isEmpty()) {
        full_session_data["systemPrompt"] = systemPromptText.toStdString();
    }
    full_session_data["conversation"] = history_json_array;


    std::ofstream ofs(filePath.toStdString());
    if (!ofs.is_open()) {
        QMessageBox::critical(this, "Save Error", "Could not open file to save history: " + filePath);
        return false;
    }
    try {
        ofs << full_session_data.dump(2); // Pretty print with indent 2
        ofs.close();
        statusBar->showMessage("History saved to: " + QFileInfo(filePath).fileName(), 3000);
        sessionLogger.logInfo("History saved to: " + filePath);
        return true;
    } catch (const std::exception& e) {
        QMessageBox::critical(this, "Save Error", QString("Error writing history JSON: %1").arg(e.what()));
        if(ofs.is_open()) ofs.close();
        return false;
    }
}

bool MainWindow::loadHistoryFromFile(const QString& filePath) {
    std::ifstream ifs(filePath.toStdString());
    if (!ifs.is_open()) {
        QMessageBox::warning(this, "Load Error", "Could not open history file: " + filePath);
        return false;
    }

    json loaded_json_data;
    try {
        ifs >> loaded_json_data;
        ifs.close();
    } catch (const json::parse_error& e) {
        QMessageBox::critical(this, "Load Error", QString("Failed to parse history JSON from %1: %2").arg(filePath, e.what()));
        if(ifs.is_open()) ifs.close();
        return false;
    } catch (const std::exception& e) {
        QMessageBox::critical(this, "Load Error", QString("Error reading history file %1: %2").arg(filePath, e.what()));
        if(ifs.is_open()) ifs.close();
        return false;
    }

    json conversation_array;
    if (loaded_json_data.contains("conversation") && loaded_json_data["conversation"].is_array()) {
        conversation_array = loaded_json_data["conversation"];
        // Optionally load system prompt, service, model from loaded_json_data here
        if (loaded_json_data.contains("systemPrompt") && loaded_json_data["systemPrompt"].is_string()){
            systemPromptText = QString::fromStdString(loaded_json_data["systemPrompt"].get<std::string>());
            // May need to call saveAppSettings() or setSystemPrompt() to update UI fully
            qInfo() << "Loaded system prompt from history:" << systemPromptText;
        }
        // Could also try to restore service and model if saved, but that's more complex interaction
    } else if (loaded_json_data.is_array()) { // Legacy format (just an array of turns)
        conversation_array = loaded_json_data;
        qWarning() << "Loading legacy history format (array of turns).";
    } else {
        QMessageBox::warning(this, "Load Error", "History file does not contain a valid conversation array.");
        return false;
    }


    std::vector<ConversationTurn> temp_history;
    try {
        for (const auto& j_turn : conversation_array) {
            temp_history.push_back(j_turn.get<ns_turn::ConversationTurn>()); // Uses ns_turn::from_json
        }
    } catch (const json::exception& e) { // More specific catch for nlohmann json
        QMessageBox::critical(this, "Load Error", QString("Invalid turn format in history file: %1").arg(e.what()));
        return false;
    } catch (const std::runtime_error& e) { // For other errors from from_json
        QMessageBox::critical(this, "Load Error", QString("Error processing turn in history file: %1").arg(e.what()));
        return false;
    }

    conversationHistory = temp_history;
    chatDisplay->clear(); // Clear current display
    supervisionConsole->clear(); // Clear supervision too

    for (const auto& turn : conversationHistory) {
        appendToChatDisplay(QString::fromStdString(turn.role), QString::fromStdString(turn.text));
    }
    if (!conversationHistory.empty() && conversationHistory.back().role == "model") {
        lastModelResponseText = QString::fromStdString(conversationHistory.back().text);
    } else {
        lastModelResponseText.clear();
    }

    statusBar->showMessage("History loaded from: " + QFileInfo(filePath).fileName(), 3000);
    sessionLogger.logInfo("History loaded from: " + filePath);
    // If system prompt was loaded, might want to show it in supervision
    if(!systemPromptText.isEmpty()) appendToSupervisionConsole("System Prompt (from loaded history)", systemPromptText);

    return true;
}


void MainWindow::loadHistory() {
    QString defaultDir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    QString filePath = QFileDialog::getOpenFileName(this, tr("Load Chat History"),
                                                    defaultDir,
                                                    tr("JSON History Files (*.json);;All Files (*)"));
    if (!filePath.isEmpty()) {
        newChat(false); // Start a new session environment (clears UI, starts new log)
                        // before loading history into it.
        loadHistoryFromFile(filePath);
    }
}

void MainWindow::saveHistory() {
    if (conversationHistory.empty()) {
        QMessageBox::information(this, "Save History", "Conversation is empty. Nothing to save.");
        return;
    }
    QString defaultName = QString("chat_session_%1.json").arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss"));
    QString defaultDir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    QString filePath = QFileDialog::getSaveFileName(this, tr("Save Chat History As"),
                                                    QDir(defaultDir).filePath(defaultName),
                                                    tr("JSON History Files (*.json);;All Files (*)"));
    if (!filePath.isEmpty()) {
        if (!filePath.endsWith(".json", Qt::CaseInsensitive)) {
            filePath += ".json";
        }
        saveHistoryToFile(filePath);
    }
}

void MainWindow::saveLastResponse() {
    if (lastModelResponseText.isEmpty()) {
        QMessageBox::information(this, "Save Last Response", "No AI response available to save.");
        return;
    }
    QString defaultName = "ai_response.txt";
    if (currentService) {
        defaultName = QString("%1_response_%2.txt").arg(currentService->getName().remove(QRegularExpression("[^a-zA-Z0-9_]")) , QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss"));
    }

    QString filePath = QFileDialog::getSaveFileName(this, tr("Save Last AI Response As"),
                                                    QDir(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)).filePath(defaultName),
                                                    tr("Text Files (*.txt);;All Files (*)"));
    if (!filePath.isEmpty()) {
        QFile file(filePath);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&file);
            out << lastModelResponseText;
            file.close();
            statusBar->showMessage("Last response saved to: " + QFileInfo(filePath).fileName(), 3000);
            sessionLogger.logInfo("Last AI response saved to: " + filePath);
        } else {
            QMessageBox::critical(this, "Save Error", "Could not open file to save response: " + filePath);
            sessionLogger.logError("Failed to save last AI response to: " + filePath + " - " + file.errorString());
        }
    }
}