// mainwindow.h

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QString>
#include <vector>
#include <string>
#include <memory> // For std::unique_ptr

// Include headers for your own classes that are direct members
#include "logging/session_logger.h"   // Adjusted path
#include "command_lib/command_library.h" // Adjusted path

// Forward declarations for Qt classes used as pointers
QT_BEGIN_NAMESPACE
class QTextEdit;
class QPlainTextEdit;
class QPushButton;
class QComboBox;
class QSplitter;
class QStatusBar;
class QAction;
class QMenu;
class QActionGroup;
class QProcess; // For terminal_process_handler if it's a member
QT_END_NAMESPACE

// Forward declarations for your custom classes if used as pointers
class LLMService;
// class OrchestratorManager; // If OrchestratorManager is a member pointer
// class TerminalProcessHandler; // If TerminalProcessHandler is a member pointer

// Definition of simple structs used within the class
struct ConversationTurn {
    std::string role;
    std::string text;
};

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private slots:
    void onSendClicked();

    void newChat(bool confirm = true);
    void loadHistory();
    void saveHistory();
    void saveLastResponse();
    void setSystemPrompt();

    void selectServiceGemini();
    void selectServiceOllama();
    void selectServiceLMStudio();
    void selectServiceVLLM();
    void configureCurrentService();
    void modelSelectionChanged(const QString& modelName);

    // Example slots for other UI elements if needed
    // void onOrchestratorStateChanged(const QString& state);
    // void handleTerminalOutput();


private:
    void setupUi();
    void setupMenus();
    void loadAppSettings();
    void saveAppSettings();
    void appendToChatDisplay(const QString& role, const QString& text);
    void appendToSupervisionConsole(const QString& title, const QString& content, bool isJson = false);

    void setActiveService(std::unique_ptr<LLMService> service);
    void fetchModelsForCurrentService();
    void processLlmResponseForCommands(const QString& llmResponseText);

    // UI Elements
    QTextEdit *chatDisplay;
    QTextEdit *promptInput;
    QPushButton *sendButton;
    QStatusBar *statusBar;
    QComboBox *modelSelector;
    QSplitter *mainSplitter;
    QPlainTextEdit *supervisionConsole;

    // Potentially other UI elements (commented out if not directly part of this iteration)
    // QPlainTextEdit *orchestratorConsoleOutput;
    // QPlainTextEdit *terminalConsoleOutput; // If you meant to have separate terminal output UI
    // QPushButton *startButton;
    // QPushButton *stopButton;
    // QPushButton *sendCommandButton;
    // QLineEdit *terminalCommandInput;

    // Actions and Menus
    QAction *newAction;
    QAction *loadHistoryAction;
    QAction *saveHistoryAction;
    QAction *saveLastResponseAction;
    QAction *exitAction;
    QAction *setSystemPromptAction;
    QAction *configureServiceAction;
    // QAction *manageTasksAction;
    // QAction *viewLogsAction;

    QMenu *serviceMenu;
    // QMenu *orchestrationMenu;
    // QMenu *toolsMenu;

    // Data Members
    QString systemPromptText;
    std::vector<ConversationTurn> conversationHistory;
    QString lastModelResponseText;

    std::unique_ptr<LLMService> currentService;
    QString m_lastSelectedServiceType;

    SessionLogger sessionLogger;
    CommandLibrary::CommandManager commandManager;

    // Other managers/handlers (if they are members)
    // std::unique_ptr<OrchestratorManager> orchestratorManager;
    // std::unique_ptr<TerminalProcessHandler> terminalHandler;
};

#endif // MAINWINDOW_H