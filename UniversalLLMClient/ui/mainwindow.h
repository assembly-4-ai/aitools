#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QString>
#include <vector>
#include <string>
#include <memory> // For std::unique_ptr

#include "common/types.h" // For ConversationTurn
#include "llm_services/llm_service.h" 
#include "command_lib/command_library.h" 
#include "logging/session_logger.h" 
#include "orchestration/orchestrator_manager.h" // For OrchestratorManager
#include "terminal_interaction/terminal_process_handler.h" // For TerminalProcessHandler

// Forward declarations for Qt classes
QT_BEGIN_NAMESPACE
class QTextEdit;
class QPushButton;
class QLineEdit;
class QStatusBar;
class QProgressDialog;
class QVBoxLayout;
class QHBoxLayout;
class QWidget;
class QComboBox;
class QMenu;
class QAction; 
class QPlainTextEdit; 
class QSplitter;      
class QTabWidget;     // Added
QT_END_NAMESPACE

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onSendClicked();

    // Slots for LLMService interaction
    void configureCurrentService();
    void modelSelectionChanged(const QString& modelName);
    void handleServiceResponse(const QString& responseText, const QString& requestPayload, const QString& fullResponsePayload, const QString& errorString);
    void handleModelsFetched(const QStringList& modelList, const QString& errorString);

    // Slots for selecting specific services
    void selectServiceGemini();
    void selectServiceOllama(); 
    void selectServiceLMStudio(); 
    void selectServiceVLLM();
    
    void onNewChat(); // New slot for "New Chat" action

    // Orchestrator UI interaction
    void onOrchestratorStartClicked();
    void onOrchestratorStopClicked();

    // Slots to handle signals from OrchestratorManager
    void handleOrchestrationStatusUpdate(const QString& statusMessage);
    void handleOrchestratorNeedsUserInput(const QString& question, const QString& suggestedResponse);
    void handleOrchestratorSuggestsLocalCommand(const QString& commandToExecute);
    void handleOrchestrationFinished(bool success, const QString& summary);
    void handleOrchestratorDisplayMessage(const QString& message, bool isError);

    // Terminal UI interaction
    void onTerminalSendCommandClicked();
    void onTerminalStartShellClicked();
    void onTerminalStopShellClicked();
    void onOpenDefaultTerminal(); // For menu action
    void onConfigureTerminalPaths(); // For menu action

    // Slots to handle signals from TerminalProcessHandler
    void handleTerminalOutputReady(const QString& output);
    void handleTerminalErrorReady(const QString& errorOutput);
    void handleTerminalProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void handleTerminalProcessErrorOccurred(QProcess::ProcessError error);

private:
    void setupUi();
    // void loadApiKey(); // To be removed or adapted if specific API key handling is no longer global
    // void saveApiKey(); // To be removed or adapted
    // void promptForApiKey(); // To be removed or adapted

    // Methods for service abstraction
    void initializeServices(); 
    void setActiveService(LLMService* service); 
    void loadAppSettings();    
    void saveAppSettings();    
    
    // New method for processing commands from LLM response
    void processLlmResponseForCommands(const QString& llmResponseText);
    void appendToSupervisionConsole(const QString& title, const QString& content, bool isJson = false);


    // Basic UI member pointers
    QTextEdit *chatDisplay;
    QLineEdit *promptInput;
    QPushButton *sendButton;
    QStatusBar *statusBar; 

    // QLineEdit *apiKeyInput; // Removed
    // QPushButton *setApiKeyButton; // Removed

    // Data members
    // QString m_apiKey; // Removed, API keys managed by individual services
    std::vector<Common::ConversationTurn> m_conversationHistory;

    QProgressDialog *m_progressDialog;    

    // Central widget and layout
    QWidget *centralWidget;
    QVBoxLayout *mainLayout;
    QHBoxLayout *inputLayout;
    // QHBoxLayout *apiKeyLayout; // Removed

    // LLM Service Abstraction
    std::unique_ptr<LLMService> m_currentService;
    QString m_lastSelectedServiceType; 
    
    // UI for service and model selection
    QMenu *m_serviceMenu;
    QComboBox *m_modelSelector;

    // Command Library Integration
    std::unique_ptr<CommandLib::CommandManager> m_commandManager;

    // Session Logger
    std::unique_ptr<SessionLogger> m_sessionLogger;

    // File Menu
    QMenu* m_fileMenu;
    QAction* m_newChatAction;

    // Supervision Console UI
    QSplitter* m_mainSplitter;
    QPlainTextEdit* m_supervisionConsole;

    // Orchestrator Integration
    std::unique_ptr<OrchestratorManager> m_orchestratorManager;
    
    // Orchestrator UI elements
    QTabWidget* m_mainTabWidget; 
    QWidget* m_orchestratorTab;  
    QPlainTextEdit* m_orchestratorConsoleOutput;
    QLineEdit* m_orchestratorTodoInput;
    QPushButton* m_orchestratorStartButton;
    QPushButton* m_orchestratorStopButton;

    // Terminal Interaction
    std::unique_ptr<TerminalProcessHandler> m_defaultTerminalHandler;
    
    // Terminal UI elements (within a new tab)
    QWidget* m_terminalsTab; // Container widget for the new Terminals tab
    QPlainTextEdit* m_terminalOutputConsole;
    QLineEdit* m_terminalInputLineEdit;
    QPushButton* m_terminalSendCommandButton;
    QPushButton* m_terminalStartShellButton; // Button to start/restart the shell
    QPushButton* m_terminalStopShellButton;  // Button to stop the shell

    // Actions for terminal menu
    QAction* m_openDefaultTerminalAction;
    QAction* m_configureTerminalPathsAction; 
};

#endif // MAINWINDOW_H
