#include "ui/mainwindow.h" 
#include "llm_services/llm_service.h" 
#include "llm_services/gemini_service.h" 
#include "llm_services/ollama_service.h" 
#include "llm_services/openai_compatible_service.h"
#include "logging/session_logger.h" 
#include "command_lib/command_library.h" 
#include "orchestration/orchestrator_manager.h" // Included via mainwindow.h, but explicit for clarity

// Standard Qt Widgets & Utilities
#include <QApplication>
#include <QStandardPaths> 
#include <QDir>           
#include <QRegularExpression> 
#include <QAction>        
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QWidget>
#include <QTextEdit>
#include <QPushButton>
#include <QLineEdit>
#include <QStatusBar>
#include <QProgressDialog>
#include <QInputDialog>   
#include <QSettings>      
#include <QMessageBox>
#include <QComboBox>      
#include <QLabel>         
#include <QMenu>          
#include <QMenuBar>       
#include <QPlainTextEdit> 
#include <QSplitter>      
#include <QFontDatabase>  
#include <QJsonDocument>  
#include <QJsonObject>    
#include <QJsonArray>     
#include <QDateTime>      
#include <QTabWidget>     // For m_mainTabWidget
// #include <QFormLayout> // Alternative, not used for now


// Standard Library
#include <string>
#include <vector>
#include <memory> // For std::unique_ptr

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), 
      chatDisplay(nullptr),
      promptInput(nullptr),
      sendButton(nullptr),
      statusBar(nullptr),
      m_progressDialog(nullptr),
      centralWidget(nullptr), // This will be the QTabWidget itself or a container for it
      mainLayout(nullptr),    // This will be the layout for the Chat UI tab's content
      inputLayout(nullptr),
      m_currentService(nullptr), 
      m_serviceMenu(nullptr),
      m_modelSelector(nullptr),
      m_fileMenu(nullptr),       
      m_newChatAction(nullptr),
      m_mainSplitter(nullptr),      
      m_supervisionConsole(nullptr),
      m_mainTabWidget(nullptr),            // Initialize new member
      m_orchestratorTab(nullptr),          // Initialize new member
      m_orchestratorConsoleOutput(nullptr),// Initialize new member
      m_orchestratorTodoInput(nullptr),    // Initialize new member
      m_orchestratorStartButton(nullptr),  // Initialize new member
      m_orchestratorStopButton(nullptr)   // Initialize new member
      // m_orchestratorManager is initialized in the body
{ 
    setupUi(); 
    loadAppSettings(); 

    QString configBasePath = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    if (configBasePath.isEmpty()) {
        configBasePath = "."; 
        QMessageBox::warning(this, "Config/Log Path Error", 
                             "Could not determine standard application data location. Config and logs will be saved in the current directory.");
    }
    QString appDataDir = configBasePath + "/UniversalLLMClient"; 
    QDir dir(appDataDir);
    if (!dir.exists()) {
        dir.mkpath("."); 
    }
    QString commandConfigPath = appDataDir + "/command_config.json";
    m_commandManager = std::make_unique<CommandLib::CommandManager>(commandConfigPath.toStdString());
    m_sessionLogger = std::make_unique<SessionLogger>(appDataDir); 

    // Initialize OrchestratorManager
    m_orchestratorManager = std::make_unique<OrchestratorManager>(
        this, 
        m_currentService.get(), // Pass initial service, acknowledge limitation
        m_commandManager.get(),
        this // Parent QObject
    );

    // Connect signals from OrchestratorManager to MainWindow slots
    connect(m_orchestratorManager.get(), &OrchestratorManager::orchestrationStatusUpdate,
            this, &MainWindow::handleOrchestrationStatusUpdate);
    connect(m_orchestratorManager.get(), &OrchestratorManager::orchestratorNeedsUserInput,
            this, &MainWindow::handleOrchestratorNeedsUserInput);
    connect(m_orchestratorManager.get(), &OrchestratorManager::orchestratorSuggestsLocalCommand,
            this, &MainWindow::handleOrchestratorSuggestsLocalCommand);
    connect(m_orchestratorManager.get(), &OrchestratorManager::orchestrationFinished,
            this, &MainWindow::handleOrchestrationFinished);
    connect(m_orchestratorManager.get(), &OrchestratorManager::orchestratorDisplayMessage,
            this, &MainWindow::handleOrchestratorDisplayMessage);
}

MainWindow::~MainWindow() {
    saveAppSettings(); 
    if (m_sessionLogger) {
        m_sessionLogger->logInfo("Application closing.");
    }
}

void MainWindow::setupUi() {
    // Create QTabWidget as the new central feature
    m_mainTabWidget = new QTabWidget(this);
    this->setCentralWidget(m_mainTabWidget); // Tab widget is now central

    // --- Main Chat Tab (incorporates previous m_mainSplitter) ---
    QWidget* mainChatTab = new QWidget(m_mainTabWidget);
    QVBoxLayout* mainChatTabLayout = new QVBoxLayout(mainChatTab); // Layout for the tab itself

    m_mainSplitter = new QSplitter(Qt::Horizontal, mainChatTab); // Splitter is now inside the tab

    QWidget* chatUiContainer = new QWidget(m_mainSplitter);
    mainLayout = new QVBoxLayout(chatUiContainer); // mainLayout for chat UI elements

    m_fileMenu = menuBar()->addMenu(tr("&File"));
    m_newChatAction = new QAction(tr("&New Chat"), this);
    connect(m_newChatAction, &QAction::triggered, this, &MainWindow::onNewChat);
    m_fileMenu->addAction(m_newChatAction);
    initializeServices(); 

    QHBoxLayout* serviceModelLayout = new QHBoxLayout();
    serviceModelLayout->addWidget(new QLabel("Model:", chatUiContainer)); 
    m_modelSelector = new QComboBox(chatUiContainer);
    m_modelSelector->setPlaceholderText("Select Model (after service)");
    m_modelSelector->setEnabled(false); 
    serviceModelLayout->addWidget(m_modelSelector, 1);
    mainLayout->addLayout(serviceModelLayout);
    connect(m_modelSelector, &QComboBox::currentTextChanged, this, &MainWindow::modelSelectionChanged);

    chatDisplay = new QTextEdit(chatUiContainer);
    chatDisplay->setReadOnly(true);
    mainLayout->addWidget(chatDisplay, 1); 

    inputLayout = new QHBoxLayout(); 
    promptInput = new QLineEdit(chatUiContainer);
    promptInput->setPlaceholderText("Enter your prompt...");
    sendButton = new QPushButton("Send", chatUiContainer);
    inputLayout->addWidget(promptInput, 1); 
    inputLayout->addWidget(sendButton);
    mainLayout->addLayout(inputLayout);
    
    chatUiContainer->setLayout(mainLayout); 
    m_mainSplitter->addWidget(chatUiContainer); 

    m_supervisionConsole = new QPlainTextEdit(m_mainSplitter);
    m_supervisionConsole->setReadOnly(true);
    m_supervisionConsole->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    m_supervisionConsole->setPlaceholderText("LLM interactions and command details will appear here...");
    
    m_mainSplitter->addWidget(m_supervisionConsole);
    
    m_mainSplitter->setStretchFactor(0, 2); 
    m_mainSplitter->setStretchFactor(1, 1); 

    mainChatTabLayout->addWidget(m_mainSplitter); // Add splitter to the tab's layout
    mainChatTab->setLayout(mainChatTabLayout);
    m_mainTabWidget->addTab(mainChatTab, "Chat & Supervision");

    // --- Orchestrator Tab ---
    m_orchestratorTab = new QWidget(m_mainTabWidget);
    QVBoxLayout* orchestratorLayout = new QVBoxLayout(m_orchestratorTab);

    m_orchestratorConsoleOutput = new QPlainTextEdit(m_orchestratorTab);
    m_orchestratorConsoleOutput->setReadOnly(true);
    m_orchestratorConsoleOutput->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    m_orchestratorConsoleOutput->setPlaceholderText("Orchestrator messages and status will appear here...");
    orchestratorLayout->addWidget(m_orchestratorConsoleOutput, 1); // Stretch factor

    QHBoxLayout* todoInputLayout = new QHBoxLayout();
    todoInputLayout->addWidget(new QLabel("ToDo List (comma-separated):", m_orchestratorTab));
    m_orchestratorTodoInput = new QLineEdit(m_orchestratorTab);
    m_orchestratorTodoInput->setPlaceholderText("e.g., research topic X, write summary, run script Y");
    todoInputLayout->addWidget(m_orchestratorTodoInput, 1);
    orchestratorLayout->addLayout(todoInputLayout);

    QHBoxLayout* orchestratorButtonsLayout = new QHBoxLayout();
    m_orchestratorStartButton = new QPushButton("Start Orchestration", m_orchestratorTab);
    m_orchestratorStopButton = new QPushButton("Stop Orchestration", m_orchestratorTab);
    m_orchestratorStopButton->setEnabled(false); // Initially disabled
    orchestratorButtonsLayout->addWidget(m_orchestratorStartButton);
    orchestratorButtonsLayout->addWidget(m_orchestratorStopButton);
    orchestratorLayout->addLayout(orchestratorButtonsLayout);

    m_orchestratorTab->setLayout(orchestratorLayout);
    m_mainTabWidget->addTab(m_orchestratorTab, "Orchestrator");

    // Connections for Orchestrator UI
    connect(m_orchestratorStartButton, &QPushButton::clicked, this, &MainWindow::onOrchestratorStartClicked);
    connect(m_orchestratorStopButton, &QPushButton::clicked, this, &MainWindow::onOrchestratorStopClicked);

    // Status Bar (common to whole window)
    statusBar = new QStatusBar(this);
    this->setStatusBar(statusBar);
    statusBar->showMessage("Ready. Please select a service.");

    // Other connections (sendButton, promptInput)
    connect(sendButton, &QPushButton::clicked, this, &MainWindow::onSendClicked);
    connect(promptInput, &QLineEdit::returnPressed, this, &MainWindow::onSendClicked);

    // Progress Dialog (common)
    m_progressDialog = new QProgressDialog("Processing...", QString(), 0, 0, this);
    m_progressDialog->setWindowModality(Qt::WindowModal);
    m_progressDialog->setAutoClose(false); 
    m_progressDialog->setAutoReset(false); 
    m_progressDialog->setCancelButton(nullptr); 
    m_progressDialog->hide(); 

    this->setWindowTitle("Universal LLM Client"); 
}

void MainWindow::appendToSupervisionConsole(const QString& title, const QString& content, bool isJson) {
    if (!m_supervisionConsole) return;

    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz");
    QString htmlOutput = QString("<h3>[%1] %2</h3>").arg(timestamp, title.toHtmlEscaped());

    if (isJson) {
        QJsonParseError parseError;
        QJsonDocument jsonDoc = QJsonDocument::fromJson(content.toUtf8(), &parseError);
        if (parseError.error == QJsonParseError::NoError) {
            htmlOutput += "<pre>" + jsonDoc.toJson(QJsonDocument::Indented).toHtmlEscaped() + "</pre>";
        } else {
            htmlOutput += "<p><i>(Invalid JSON, showing raw content)</i></p>";
            htmlOutput += "<pre>" + content.toHtmlEscaped() + "</pre>";
        }
    } else {
        htmlOutput += "<pre>" + content.toHtmlEscaped() + "</pre>";
    }
    htmlOutput += "<hr>"; 

    m_supervisionConsole->appendHtml(htmlOutput);
}

void MainWindow::initializeServices() {
    m_serviceMenu = menuBar()->addMenu(tr("&Services"));
    QAction* selectGeminiAction = new QAction(tr("Select &Gemini"), this);
    connect(selectGeminiAction, &QAction::triggered, this, &MainWindow::selectServiceGemini);
    m_serviceMenu->addAction(selectGeminiAction);
    QAction* selectOllamaAction = new QAction(tr("Select &Ollama"), this);
    connect(selectOllamaAction, &QAction::triggered, this, &MainWindow::selectServiceOllama);
    m_serviceMenu->addAction(selectOllamaAction);
    QAction* selectLMStudioAction = new QAction(tr("Select LM Studio"), this);
    connect(selectLMStudioAction, &QAction::triggered, this, &MainWindow::selectServiceLMStudio);
    m_serviceMenu->addAction(selectLMStudioAction);
    QAction* selectVLLMAction = new QAction(tr("Select VLLM"), this);
    connect(selectVLLMAction, &QAction::triggered, this, &MainWindow::selectServiceVLLM);
    m_serviceMenu->addAction(selectVLLMAction);
    m_serviceMenu->addSeparator();
    QAction* configureServiceAction = new QAction(tr("&Configure Current Service..."), this);
    connect(configureServiceAction, &QAction::triggered, this, &MainWindow::configureCurrentService);
    m_serviceMenu->addAction(configureServiceAction);
}

void MainWindow::loadAppSettings() {
    QSettings settings(QCoreApplication::organizationName(), QCoreApplication::applicationName());
    m_lastSelectedServiceType = settings.value("MainWindow/lastSelectedServiceType", "").toString();
    if (m_lastSelectedServiceType == "Google Gemini") { selectServiceGemini(); } 
    else if (m_lastSelectedServiceType == "Ollama") { selectServiceOllama(); } 
    else if (m_lastSelectedServiceType == "LM Studio") { selectServiceLMStudio(); } 
    else if (m_lastSelectedServiceType == "VLLM") { selectServiceVLLM(); }
    else { statusBar->showMessage("No default service selected. Please select a service from the menu."); }
}

void MainWindow::saveAppSettings() {
    QSettings settings(QCoreApplication::organizationName(), QCoreApplication::applicationName());
    settings.setValue("MainWindow/lastSelectedServiceType", m_lastSelectedServiceType);
    if (m_currentService) {
        settings.setValue("MainWindow/lastSelectedModelFor_" + m_lastSelectedServiceType, m_currentService->getLastSelectedModel());
    }
}

void MainWindow::setActiveService(LLMService* service) {
    if (m_orchestratorManager && m_orchestratorManager->m_state != OrchestratorManager::OrchestrationState::Idle) {
        QMessageBox::warning(this, "Orchestration Active", 
                                "An orchestration task is currently running. Please stop it before changing the LLM service.");
        delete service; // Clean up the newly created service as it won't be used
        return; 
    }

    if (m_currentService.get() == service && service != nullptr) { 
        if (!service->isConfigured()){ statusBar->showMessage(service->getName() + " is not configured properly."); } 
        else { m_currentService->fetchModels(this); }
        return;
    }
    if (m_currentService) {
        disconnect(m_currentService.get(), &LLMService::responseReady, this, &MainWindow::handleServiceResponse);
        disconnect(m_currentService.get(), &LLMService::modelsFetched, this, &MainWindow::handleModelsFetched);
    }
    m_currentService.reset(service); 
    m_modelSelector->clear();
    m_modelSelector->setEnabled(false);
    m_modelSelector->setPlaceholderText("Select Model");

    if (m_orchestratorManager && m_currentService) {
        // Update the orchestrator's LLM service.
        // This assumes OrchestratorManager has a method like setLlmService,
        // or it's handled via its constructor if it's re-created.
        // For now, we'll assume the orchestrator continues using the service it was initialized with,
        // or this needs a specific method in OrchestratorManager.
        // For Phase 7, this line would be: m_orchestratorManager->setLlmService(m_currentService.get());
        // As setLlmService is not defined, we comment it out and accept the limitation
        // that orchestrator uses the service active at its construction or needs a dedicated one.
    }

    if (m_currentService) {
        connect(m_currentService.get(), &LLMService::responseReady, this, &MainWindow::handleServiceResponse);
        connect(m_currentService.get(), &LLMService::modelsFetched, this, &MainWindow::handleModelsFetched);
        m_lastSelectedServiceType = m_currentService->getName(); 
        setWindowTitle("Universal LLM Client - " + m_currentService->getName());
        statusBar->showMessage("Selected service: " + m_currentService->getName());
        if (m_sessionLogger) {
            m_sessionLogger->logInfo(QString("Service selected: %1. Configured: %2")
                                       .arg(m_currentService->getName()).arg(m_currentService->isConfigured() ? "Yes" : "No"));
        }
        if (m_currentService->isConfigured()) { m_currentService->fetchModels(this); } 
        else {
            statusBar->showMessage(m_currentService->getName() + " service is not configured. Please configure it from the Services menu.");
            QMessageBox::information(this, "Service Not Configured", m_currentService->getName() + " service requires configuration before use.");
        }
    } else {
        m_lastSelectedServiceType.clear();
        setWindowTitle("Universal LLM Client");
        statusBar->showMessage("No service selected.");
        if (m_sessionLogger) m_sessionLogger->logInfo("No service selected (service deselected or cleared).");
    }
    saveAppSettings(); 
}

void MainWindow::selectServiceGemini() { setActiveService(new GeminiService(this)); }
void MainWindow::selectServiceOllama() { setActiveService(new OllamaService(this)); }
void MainWindow::selectServiceLMStudio() { setActiveService(new OpenAICompatibleService("LM Studio", "http://localhost:1234/v1", this)); }
void MainWindow::selectServiceVLLM() { setActiveService(new OpenAICompatibleService("VLLM", "http://localhost:8000/v1", this));}

void MainWindow::configureCurrentService() {
    if (m_currentService) {
        m_currentService->configureService(this);
        if (m_sessionLogger && m_currentService) {
            m_sessionLogger->logInfo(QString("Configuration attempted for service: %1. Is configured: %2")
                                       .arg(m_currentService->getName()).arg(m_currentService->isConfigured() ? "Yes" : "No"));
        }
        if(m_currentService->isConfigured()){
            statusBar->showMessage("Re-fetching models for " + m_currentService->getName() + " after configuration.");
            m_currentService->fetchModels(this);
        } else {
            statusBar->showMessage(m_currentService->getName() + " is still not configured.");
            m_modelSelector->clear(); m_modelSelector->setEnabled(false); m_modelSelector->setPlaceholderText("Service not configured");
        }
    } else { QMessageBox::information(this, "No Service", "Please select a service first."); }
}

void MainWindow::onSendClicked() {
    QString promptText = promptInput->text().trimmed();
    if (m_sessionLogger) m_sessionLogger->logUserPrompt(promptText);
    if (promptText.isEmpty()) { QMessageBox::warning(this, "Empty Prompt", "Please enter a prompt."); return; }
    if (!m_currentService) { QMessageBox::warning(this, "No Service", "Please select an LLM service from the Services menu."); return; }
    if (!m_currentService->isConfigured()) {
        QMessageBox::warning(this, "Service Not Configured", m_currentService->getName() + " is not configured. Please configure it via the Services menu."); return; 
    }
    if (m_modelSelector->isEnabled() && m_modelSelector->currentText().isEmpty()){
        QMessageBox::warning(this, "No Model Selected", "Please select a model from the dropdown list."); return;
    }
    chatDisplay->append("<b>You:</b> " + promptText);
    m_conversationHistory.push_back({"user", promptText.toStdString()}); 
    promptInput->clear();
    if (m_progressDialog) {
        m_progressDialog->setLabelText("Calling " + m_currentService->getName() + "...");
        m_progressDialog->show();
    }
    statusBar->showMessage("Sending prompt to " + m_currentService->getName() + "...");
    m_currentService->generateResponse(m_conversationHistory, promptText, this);
}

void MainWindow::handleServiceResponse(const QString& responseText, const QString& requestPayload, 
                                       const QString& fullResponsePayload, const QString& errorString) {
    if (m_progressDialog) { m_progressDialog->hide(); }
    QString serviceDisplayName = m_currentService ? m_currentService->getName() : "Service";
    QString modelName = (m_currentService && !m_currentService->getLastSelectedModel().isEmpty()) ? 
                        m_currentService->getLastSelectedModel() : "default";

    if (m_sessionLogger && m_currentService) {
        m_sessionLogger->logLlmRequest(serviceDisplayName, modelName, requestPayload); 
        m_sessionLogger->logLlmResponse(serviceDisplayName, modelName, responseText, fullResponsePayload, errorString);
    }
    if (!requestPayload.isEmpty()) {
         appendToSupervisionConsole(QString("LLM Request Payload (%1 / %2)").arg(serviceDisplayName, modelName), requestPayload, true);
    }
    if (!errorString.isEmpty()) {
        chatDisplay->append(QString("<font color='red'><b>Error from %1:</b> %2</font>").arg(serviceDisplayName, errorString));
        QMessageBox::critical(this, "Service Error", errorString);
        statusBar->showMessage("Service Error: " + errorString, 5000);
        appendToSupervisionConsole(QString("LLM Error (%1 / %2)").arg(serviceDisplayName, modelName), errorString, false);
        if (!fullResponsePayload.isEmpty()){ 
            appendToSupervisionConsole(QString("LLM Full Response (on error) (%1 / %2)").arg(serviceDisplayName, modelName), fullResponsePayload, true);
        }
    } else {
        QString chatServiceName = serviceDisplayName.split(" ").first(); 
        chatDisplay->append(QString("<b>%1:</b> %2").arg(chatServiceName, responseText));
        m_conversationHistory.push_back({"model", responseText.toStdString()}); 
        statusBar->showMessage("Response received from " + serviceDisplayName + ".", 3000);
        if (!fullResponsePayload.isEmpty()){ 
            appendToSupervisionConsole(QString("LLM Full Response (%1 / %2)").arg(serviceDisplayName, modelName), fullResponsePayload, true);
        }
        if (!responseText.isEmpty()) { processLlmResponseForCommands(responseText); }
    }
}

void MainWindow::handleModelsFetched(const QStringList& modelList, const QString& errorString) {
    m_modelSelector->clear();
    QString serviceName = m_currentService ? m_currentService->getName() : "selected service";
    if (m_sessionLogger && m_currentService) {
        if (!errorString.isEmpty()) {
            m_sessionLogger->logError(QString("Failed to fetch models for %1: %2").arg(m_currentService->getName()).arg(errorString));
        } else {
            m_sessionLogger->logInfo(QString("Models fetched for %1: %2 models found (%3)")
                                       .arg(m_currentService->getName()).arg(modelList.size()).arg(modelList.join(", ")));
        }
    }
    if (!errorString.isEmpty()) {
        statusBar->showMessage("Error fetching models for " + serviceName + ": " + errorString, 5000);
        QMessageBox::warning(this, "Model Fetching Error", "Could not fetch models for " + serviceName + ":\n" + errorString);
        m_modelSelector->setEnabled(false); m_modelSelector->setPlaceholderText("Error fetching models");
    } else if (modelList.isEmpty()) {
        statusBar->showMessage("No models available for " + serviceName + ".", 3000);
        m_modelSelector->setEnabled(false); m_modelSelector->setPlaceholderText("No models available");
    } else {
        m_modelSelector->addItems(modelList);
        m_modelSelector->setEnabled(true); m_modelSelector->setPlaceholderText("Select Model");
        statusBar->showMessage("Models loaded for " + serviceName + ".", 3000);
        QSettings settings(QCoreApplication::organizationName(), QCoreApplication::applicationName());
        QString lastModelKey = "MainWindow/lastSelectedModelFor_" + (m_currentService ? m_currentService->getName() : "");
        QString lastModelForService = settings.value(lastModelKey, "").toString();
        if (!lastModelForService.isEmpty() && modelList.contains(lastModelForService)) { m_modelSelector->setCurrentText(lastModelForService); } 
        else if (!modelList.isEmpty()) { m_modelSelector->setCurrentIndex(0); }
        if(m_modelSelector->count() > 0) modelSelectionChanged(m_modelSelector->currentText());
    }
}

void MainWindow::processLlmResponseForCommands(const QString& llmResponseText) {
    QRegularExpression commandRegex(R"((?:^|\n|`)\s*COMMAND:\s*(\S+)(?:\s+(.*))?\s*(?:$|\n|`))");
    QRegularExpressionMatchIterator i = commandRegex.globalMatch(llmResponseText);
    while (i.hasNext()) {
        QRegularExpressionMatch match = i.next();
        QString commandName = match.captured(1).trimmed();
        QString commandArgs = match.captured(2).trimmed(); 
        QString fullCapturedCommand = commandName + (commandArgs.isEmpty() ? "" : " " + commandArgs);
        appendToSupervisionConsole("LLM Suggested Command", fullCapturedCommand, false); 
        if (commandName.isEmpty()) continue;
        QMessageBox::StandardButton reply;
        reply = QMessageBox::question(this, "Execute Local Command?", 
                                      QString("The LLM suggests executing the following local command:\n\n%1\n\nDo you want to allow this?").arg(fullCapturedCommand),
                                      QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel, QMessageBox::No); 
        if (reply == QMessageBox::Yes) {
            appendToSupervisionConsole("User Confirmation", "User APPROVED execution of: " + fullCapturedCommand, false);
            std::pair<std::string, std::string> result = m_commandManager->executeCommand(fullCapturedCommand.toStdString());
            if (m_sessionLogger) {
                m_sessionLogger->logLocalCommand(fullCapturedCommand, QString::fromStdString(result.first), QString::fromStdString(result.second), true); 
            }
            QString output = QString::fromStdString(result.first);
            QString error = QString::fromStdString(result.second);
            QString systemMessageText;
            QString supervisionOutput = "Executed Command: " + fullCapturedCommand + "\nOutput:\n" + output + "\nError:\n" + error;
            appendToSupervisionConsole("Local Command Execution Result", supervisionOutput, false);
            if (!error.isEmpty()) {
                chatDisplay->append(QString("<font color='orange'><b>System (Command Error for '%1'):</b> %2</font>").arg(commandName, error));
                systemMessageText = QString("Error executing command '%1': %2").arg(commandName, error);
            } else if (!output.isEmpty()) {
                chatDisplay->append(QString("<font color='green'><b>System (Command Output for '%1'):</b><br>%2</font>").arg(commandName, output.toHtmlEscaped()));
                systemMessageText = QString("Executed command '%1', result:\n%2").arg(commandName, output);
            } else {
                chatDisplay->append(QString("<font color='blue'><b>System (Command '%1' executed with no output.)</b></font>").arg(commandName));
                systemMessageText = QString("Command '%1' executed with no output.").arg(commandName);
            }
            m_conversationHistory.push_back({"system", systemMessageText.toStdString()});
        } else if (reply == QMessageBox::No) {
            appendToSupervisionConsole("User Confirmation", "User DENIED execution of: " + fullCapturedCommand, false);
            QString cancelledMsg = QString("Execution of command '%1' denied by user.").arg(fullCapturedCommand);
            if (m_sessionLogger) { m_sessionLogger->logLocalCommand(fullCapturedCommand, "", "User denied execution.", false); }
            chatDisplay->append("<font color='gray'><i>" + cancelledMsg + "</i></font>");
            m_conversationHistory.push_back({"system", cancelledMsg.toStdString()});
        } else { 
            appendToSupervisionConsole("User Confirmation", "User CANCELLED decision for: " + fullCapturedCommand, false);
            QString abortedMsg = QString("Command execution decision aborted by user for: '%1'.").arg(fullCapturedCommand);
            if (m_sessionLogger) { m_sessionLogger->logLocalCommand(fullCapturedCommand, "", "User cancelled/aborted decision.", false); }
            chatDisplay->append("<font color='gray'><i>" + abortedMsg + "</i></font>");
        }
    }
}

void MainWindow::modelSelectionChanged(const QString& modelName) {
    if (m_currentService && !modelName.isEmpty() && m_modelSelector->isEnabled()) {
        m_currentService->setLastSelectedModel(modelName);
        QSettings settings(QCoreApplication::organizationName(), QCoreApplication::applicationName());
        settings.setValue("MainWindow/lastSelectedModelFor_" + m_currentService->getName(), modelName);
        statusBar->showMessage("Model set to: " + modelName, 2000);
        if (m_sessionLogger) {
             m_sessionLogger->logInfo(QString("Model changed for service %1 to: %2")
                                   .arg(m_currentService->getName()).arg(modelName));
        }
    }
}

void MainWindow::onNewChat() {
    if (m_sessionLogger) {
        if (m_sessionLogger->startNewSessionLog()) {
            statusBar->showMessage("New chat session started. Log file created.", 3000);
        } else {
            statusBar->showMessage("Error starting new log session.", 3000);
            QMessageBox::warning(this, "Logging Error", "Could not start a new log session. Check permissions or disk space.");
        }
    }
    m_conversationHistory.clear();
    chatDisplay->clear();
    if (m_supervisionConsole) m_supervisionConsole->clear(); 
    if (m_orchestratorConsoleOutput) m_orchestratorConsoleOutput->clear(); // Clear orchestrator console too
    if (m_currentService) {
         if (m_sessionLogger) m_sessionLogger->logInfo(QString("New chat started with service: %1, Model: %2")
                               .arg(m_currentService->getName())
                               .arg(m_currentService->getLastSelectedModel()));
    } else {
         if (m_sessionLogger) m_sessionLogger->logInfo("New chat started. No service selected.");
    }
}

// --- Orchestrator UI interaction slots ---
void MainWindow::onOrchestratorStartClicked() {
    if (!m_orchestratorManager) return;
    QString todoText = m_orchestratorTodoInput->text().trimmed();
    if (todoText.isEmpty()) {
        QMessageBox::warning(m_orchestratorTab, "Empty ToDo List", "Please enter a list of tasks for the orchestrator.");
        return;
    }
    QStringList todoList = todoText.split(',', Qt::SkipEmptyParts); 
    for(QString& task : todoList) task = task.trimmed(); 

    if (todoList.isEmpty()) {
        QMessageBox::warning(m_orchestratorTab, "Empty ToDo List", "Please enter valid tasks, separated by commas.");
        return;
    }
    
    if (m_currentService && m_orchestratorManager) {
        // Update orchestrator's LLM service if it's different or not set.
        // This is a simplification. A more robust approach might involve a dedicated service instance for the orchestrator
        // or a mechanism to ensure the service doesn't change mid-orchestration if it's shared.
        // For now, we pass the currently active service.
        // m_orchestratorManager->setLlmService(m_currentService.get()); // If such setter existed
    }
    if (!m_orchestratorManager->m_llmService || !m_orchestratorManager->m_llmService->isConfigured()) {
         QMessageBox::warning(this, "Orchestrator LLM Service Error", "The LLM service for the orchestrator is not configured or unavailable. Please select and configure a service in the Chat tab first.");
         return;
    }


    m_orchestratorManager->startOrchestration(todoList);
    m_orchestratorStartButton->setEnabled(false);
    m_orchestratorStopButton->setEnabled(true);
    m_orchestratorTodoInput->setEnabled(false);
    if (m_orchestratorConsoleOutput) m_orchestratorConsoleOutput->clear(); // Clear previous run
    if (m_sessionLogger) m_sessionLogger->logInfo("Orchestration started with tasks: " + todoText);

}

void MainWindow::onOrchestratorStopClicked() {
    if (!m_orchestratorManager) return;
    m_orchestratorManager->stopOrchestration();
    m_orchestratorStartButton->setEnabled(true);
    m_orchestratorStopButton->setEnabled(false);
    m_orchestratorTodoInput->setEnabled(true);
    if (m_sessionLogger) m_sessionLogger->logInfo("Orchestration stopped by user.");
}

// --- Slots to handle signals from OrchestratorManager ---
void MainWindow::handleOrchestrationStatusUpdate(const QString& statusMessage) {
    if (m_orchestratorConsoleOutput) m_orchestratorConsoleOutput->appendPlainText(QDateTime::currentDateTime().toString("[HH:mm:ss] ") + statusMessage);
    statusBar->showMessage("Orchestrator: " + statusMessage, 3000);
}

void MainWindow::handleOrchestratorDisplayMessage(const QString& message, bool isError) {
    if (m_orchestratorConsoleOutput) {
        QString prefix = QDateTime::currentDateTime().toString("[HH:mm:ss] ");
        QString formattedMessage = (isError ? "<font color='red'>[Orchestrator Error] " : "[Orchestrator] ") 
                                 + message.toHtmlEscaped() + "</font>";
        m_orchestratorConsoleOutput->appendHtml(prefix + formattedMessage);
    }
}

void MainWindow::handleOrchestratorNeedsUserInput(const QString& question, const QString& suggestedResponse) {
    if (m_orchestratorConsoleOutput) {
        m_orchestratorConsoleOutput->appendPlainText(QDateTime::currentDateTime().toString("[HH:mm:ss] ") + QString("[Orchestrator ActionNeeded] Asking user: %1").arg(question));
    }
    bool ok;
    QString userInput = QInputDialog::getText(this, "Orchestrator Needs Input", question, 
                                              QLineEdit::Normal, suggestedResponse, &ok);
    if (m_orchestratorManager) { // Check if manager still exists (e.g. app not closing)
        if (ok) {
            m_orchestratorManager->provideUserInput(userInput);
        } else {
            m_orchestratorManager->provideUserInput(""); 
            if (m_orchestratorConsoleOutput) {
                 m_orchestratorConsoleOutput->appendPlainText(QDateTime::currentDateTime().toString("[HH:mm:ss] ") +"[Orchestrator Info] User cancelled input dialog.");
            }
        }
    }
}

void MainWindow::handleOrchestratorSuggestsLocalCommand(const QString& commandToExecute) {
    if (m_orchestratorConsoleOutput) {
        m_orchestratorConsoleOutput->appendPlainText(QDateTime::currentDateTime().toString("[HH:mm:ss] ") +QString("[Orchestrator ActionNeeded] Suggests command: %1").arg(commandToExecute));
    }
    appendToSupervisionConsole("Orchestrator Suggested Command", commandToExecute, false);

    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(this, "Orchestrator: Execute Local Command?", 
                                  QString("The Orchestrator suggests executing the local command:\n\n%1\n\nDo you want to allow this?").arg(commandToExecute),
                                  QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel,
                                  QMessageBox::No);

    QString output, error;
    // bool executed = false; // Not used

    if (reply == QMessageBox::Yes) {
        appendToSupervisionConsole("User Confirmation for Orchestrator", "User APPROVED execution of: " + commandToExecute, false);
         if (m_sessionLogger) m_sessionLogger->logLocalCommand(commandToExecute, "", "User Approved (Orchestrator)", true);

        std::pair<std::string, std::string> result = m_commandManager->executeCommand(commandToExecute.toStdString());
        output = QString::fromStdString(result.first);
        error = QString::fromStdString(result.second);
        // executed = true;
        
        QString supervisionOutput = "Executed Command (Orchestrator): " + commandToExecute + "\nOutput:\n" + output + "\nError:\n" + error;
        appendToSupervisionConsole("Local Command Execution Result (Orchestrator)", supervisionOutput, false);
        if (m_sessionLogger) m_sessionLogger->logLocalCommand(commandToExecute, output, error, true);

    } else if (reply == QMessageBox::No) {
        appendToSupervisionConsole("User Confirmation for Orchestrator", "User DENIED execution of: " + commandToExecute, false);
        error = "User denied execution.";
        if (m_sessionLogger) m_sessionLogger->logLocalCommand(commandToExecute, "", error, false);
    } else { // Cancelled
        appendToSupervisionConsole("User Confirmation for Orchestrator", "User CANCELLED decision for: " + commandToExecute, false);
        error = "User cancelled command execution decision.";
        if (m_sessionLogger) m_sessionLogger->logLocalCommand(commandToExecute, "", error, false);
    }
    
    if(m_orchestratorManager) { 
         m_orchestratorManager->processExternalCommandResult(commandToExecute, output, error);
    }
}

void MainWindow::handleOrchestrationFinished(bool success, const QString& summary) {
    QString message = QString("Orchestration Finished. Success: %1. Summary: %2")
                      .arg(success ? "Yes" : "No", summary);
    if (m_orchestratorConsoleOutput) {
        m_orchestratorConsoleOutput->appendPlainText(QDateTime::currentDateTime().toString("\n[HH:mm:ss] ") + message + "\n");
    }
    QMessageBox::information(this, "Orchestration Complete", message);
    m_orchestratorStartButton->setEnabled(true);
    m_orchestratorStopButton->setEnabled(false);
    m_orchestratorTodoInput->setEnabled(true);
    if (m_sessionLogger) m_sessionLogger->logInfo("Orchestration finished. Success: " + QString(success ? "Yes" : "No") + ". Summary: " + summary);
}
