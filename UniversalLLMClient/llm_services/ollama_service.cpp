// Define for httplib SSL support, though Ollama is often HTTP. Good for consistency.
#define CPPHTTPLIB_OPENSSL_SUPPORT 
#include "vendor/nlohmann/json.hpp" 
#include "llm_services/ollama_service.h"
#include "common/types.h"

#include <QInputDialog>
#include <QSettings>
#include <QWidget>
#include <QMessageBox>
#include <QtConcurrent/QtConcurrent>
#include <QUrl> // For URL parsing and manipulation
#include <QCoreApplication> // For QSettings

#include "vendor/httplib.h"

OllamaService::OllamaService(QObject *parent) 
    : LLMService(parent), m_dialogParent(nullptr) {
    loadSettings(); // Load base URL upon construction

    connect(&m_generateWatcher, &QFutureWatcher<QVariantMap>::finished, 
            this, &OllamaService::handleGenerateFinished);
    connect(&m_fetchModelsWatcher, &QFutureWatcher<QPair<QStringList, QString>>::finished, 
            this, &OllamaService::handleFetchModelsFinished);
}

QString OllamaService::getName() const {
    return "Ollama";
}

bool OllamaService::isConfigured() {
    return !m_baseUrl.isEmpty() && QUrl(m_baseUrl).isValid();
}

void OllamaService::loadSettings() {
    QSettings settings(QCoreApplication::organizationName(), QCoreApplication::applicationName());
    settings.beginGroup("OllamaService");
    m_baseUrl = settings.value("baseUrl", "http://localhost:11434").toString();
    m_lastSelectedModel = settings.value("lastSelectedModel", "").toString();
    settings.endGroup();
}

void OllamaService::saveSettings() {
    QSettings settings(QCoreApplication::organizationName(), QCoreApplication::applicationName());
    settings.beginGroup("OllamaService");
    settings.setValue("baseUrl", m_baseUrl);
    settings.setValue("lastSelectedModel", m_lastSelectedModel);
    settings.endGroup();
}

void OllamaService::configureService(QWidget* parent) {
    m_dialogParent = parent;
    bool ok;
    QString currentUrl = m_baseUrl;

    QString url = QInputDialog::getText(parent, tr("Configure Ollama Service"),
                                        tr("Ollama Base URL (e.g., http://localhost:11434):"), 
                                        QLineEdit::Normal, currentUrl, &ok);
    if (ok && !url.isEmpty()) {
        QUrl qurl(url);
        if (qurl.isValid() && !qurl.host().isEmpty()) {
            m_baseUrl = qurl.toString();
            if (m_baseUrl.endsWith("/")) { // Remove trailing slash if present for consistency
                m_baseUrl.chop(1);
            }
        } else {
            QMessageBox::warning(parent, "Invalid URL", "The entered URL is not valid. Please include scheme (http/https) and host.");
            // Keep old m_baseUrl or clear it? For now, keep old.
            // If you want to force re-input on invalid: m_baseUrl = currentUrl; configureService(parent); return;
        }
    } else if (ok && url.isEmpty()) {
        // User cleared the URL
        QMessageBox::StandardButton reply;
        reply = QMessageBox::question(parent, "Clear Base URL?", 
                                      "Are you sure you want to clear the Base URL? This will disable the Ollama service.",
                                      QMessageBox::Yes|QMessageBox::No);
        if (reply == QMessageBox::Yes) {
            m_baseUrl.clear();
        }
    }
    // If user cancelled, current m_baseUrl remains.

    saveSettings();

    if (!isConfigured()) {
        QMessageBox::warning(parent, "Configuration Incomplete", "Ollama Service is not fully configured (Base URL is missing or invalid).");
    } else {
        QMessageBox::information(parent, "Configuration Saved", "Ollama Service settings saved.");
    }
}

void OllamaService::fetchModels(QWidget* parent) {
    m_dialogParent = parent;
    if (!isConfigured()) {
        emit modelsFetched(QStringList(), "Ollama service not configured (Base URL missing).");
        return;
    }

    QString currentBaseUrl = m_baseUrl; // Capture for lambda

    QFuture<QPair<QStringList, QString>> future = QtConcurrent::run([currentBaseUrl]() {
        QStringList modelNames;
        QString errorString;
        QUrl url(currentBaseUrl + "/api/tags"); // Endpoint for listing models

        try {
            httplib::Client cli(url.host().toStdString().c_str(), url.port(url.scheme().toLower() == "https" ? 443 : 80));
            if (url.scheme().toLower() == "https") {
                cli.enable_server_certificate_verification(true); // Enable for HTTPS
            }
            cli.set_connection_timeout(5); // 5 seconds
            cli.set_read_timeout(15);      // 15 seconds

            auto res = cli.Get(url.path().toStdString().c_str());

            if (!res) {
                errorString = "HTTP client error: " + httplib::to_string(res.error());
                return QPair<QStringList, QString>(modelNames, errorString);
            }

            if (res->status != 200) {
                errorString = "API Error: HTTP " + QString::number(res->status) + ". " + QString::fromStdString(res->body);
                return QPair<QStringList, QString>(modelNames, errorString);
            }

            nlohmann::json jsonResponse = nlohmann::json::parse(res->body);
            if (jsonResponse.contains("models") && jsonResponse["models"].is_array()) {
                for (const auto& model : jsonResponse["models"]) {
                    if (model.contains("name") && model["name"].is_string()) {
                        modelNames.append(QString::fromStdString(model["name"].get<std::string>()));
                    }
                }
            } else {
                errorString = "Invalid JSON response format: 'models' array not found.";
            }

        } catch (const nlohmann::json::parse_error& e) {
            errorString = "JSON Parse Error (Models): " + QString::fromUtf8(e.what());
        } catch (const nlohmann::json::exception& e) {
            errorString = "JSON Data Error (Models): " + QString::fromUtf8(e.what());
        } catch (const std::exception& e) {
            errorString = "Standard Exception (Models): " + QString::fromUtf8(e.what());
        } catch (...) {
            errorString = "Unknown error fetching models.";
        }
        
        modelNames.sort(Qt::CaseInsensitive); // Sort model names alphabetically
        return QPair<QStringList, QString>(modelNames, errorString);
    });
    m_fetchModelsWatcher.setFuture(future);
}

void OllamaService::handleFetchModelsFinished() {
    QPair<QStringList, QString> result = m_fetchModelsWatcher.result();
    emit modelsFetched(result.first, result.second);
}

void OllamaService::generateResponse(const std::vector<Common::ConversationTurn>& history, 
                                     const QString& promptText, // promptText is the latest, history is prior conversation
                                     QWidget* parentWidget) {
    m_dialogParent = parentWidget;
    if (!isConfigured()) {
        emit responseReady("", "", "", "Ollama service not configured (Base URL missing).");
        return;
    }
    if (getLastSelectedModel().isEmpty()){
        emit responseReady("", "", "", "No model selected for Ollama service.");
        return;
    }

    QString currentBaseUrl = m_baseUrl;
    QString selectedModel = getLastSelectedModel();
    std::vector<Common::ConversationTurn> historyCopy = history; // Includes the latest user prompt

    QFuture<QVariantMap> future = QtConcurrent::run([currentBaseUrl, selectedModel, historyCopy]() {
        QVariantMap resultData;
        nlohmann::json request_payload_json;
        
        try {
            nlohmann::json messages_json = nlohmann::json::array();
            for (const auto& turn : historyCopy) {
                std::string role = turn.role;
                // Ollama typically uses "user", "assistant", "system".
                // "model" from ConversationTurn should map to "assistant".
                if (role == "model") role = "assistant"; 
                // System prompt might be handled differently or as first message.
                // For now, direct mapping.
                messages_json.push_back({{"role", role}, {"content", turn.text}});
            }

            request_payload_json["model"] = selectedModel.toStdString();
            request_payload_json["messages"] = messages_json;
            request_payload_json["stream"] = false; // Required by prompt

            std::string request_body_str = request_payload_json.dump();
            resultData["requestPayload"] = QString::fromStdString(request_body_str);

            QUrl url(currentBaseUrl + "/api/chat");
            httplib::Client cli(url.host().toStdString().c_str(), url.port(url.scheme().toLower() == "https" ? 443 : 80));
             if (url.scheme().toLower() == "https") {
                cli.enable_server_certificate_verification(true);
            }
            cli.set_connection_timeout(10); // 10 seconds
            cli.set_read_timeout(120);     // 2 minutes for potentially long responses

            httplib::Headers headers = { {"Content-Type", "application/json"} };
            auto http_res = cli.Post(url.path().toStdString().c_str(), headers, request_body_str, "application/json");

            if (!http_res) {
                resultData["errorString"] = "HTTP client error: " + httplib::to_string(http_res.error());
                return resultData;
            }
            
            resultData["fullResponsePayload"] = QString::fromStdString(http_res->body);

            if (http_res->status != 200) {
                resultData["errorString"] = "API Error: HTTP " + QString::number(http_res->status) + ". " + QString::fromStdString(http_res->body);
                return resultData;
            }

            nlohmann::json response_json = nlohmann::json::parse(http_res->body);
            // Ollama response structure for non-streaming chat:
            // { "model": "...", "created_at": "...", "message": { "role": "assistant", "content": "..." }, "done": true, ... }
            if (response_json.contains("message") && response_json["message"].is_object()) {
                const auto& message_obj = response_json["message"];
                if (message_obj.contains("content") && message_obj["content"].is_string()) {
                    resultData["responseText"] = QString::fromStdString(message_obj["content"].get<std::string>());
                } else { resultData["errorString"] = "API Response: 'message.content' field missing or not a string."; }
            } else { resultData["errorString"] = "API Response: 'message' object not found."; }

        } catch (const nlohmann::json::parse_error& e) {
            resultData["errorString"] = "JSON Parse Error (Ollama Response): " + QString::fromUtf8(e.what());
        } catch (const nlohmann::json::exception& e) { 
            resultData["errorString"] = "JSON Data Error (Ollama Response): " + QString::fromUtf8(e.what());
        } catch (const std::exception& e) {
            resultData["errorString"] = "Standard Exception (Ollama Call): " + QString::fromUtf8(e.what());
        } catch (...) {
            resultData["errorString"] = "Unknown error in Ollama API call thread.";
        }
        return resultData;
    });

    m_generateWatcher.setFuture(future);
}

void OllamaService::handleGenerateFinished() {
    QVariantMap result = m_generateWatcher.result();
    emit responseReady(
        result.value("responseText").toString(),
        result.value("requestPayload").toString(),
        result.value("fullResponsePayload").toString(),
        result.value("errorString").toString()
    );
}
```
