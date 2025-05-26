#define CPPHTTPLIB_OPENSSL_SUPPORT
#include "vendor/nlohmann/json.hpp"
#include "llm_services/openai_compatible_service.h"
#include "common/types.h"

#include <QInputDialog>
#include <QSettings>
#include <QWidget>
#include <QMessageBox>
#include <QtConcurrent/QtConcurrent>
#include <QUrl>
#include <QRegularExpression> // For sanitizing service name
#include <QCoreApplication> // For QSettings

#include "vendor/httplib.h"

OpenAICompatibleService::OpenAICompatibleService(const QString& serviceDisplayName, 
                                                 const QString& defaultBaseUrl, 
                                                 QObject *parent) 
    : LLMService(parent), 
      m_serviceDisplayName(serviceDisplayName),
      m_defaultBaseUrl(defaultBaseUrl),
      m_dialogParent(nullptr) {
    m_serviceSettingsKey = sanitizeServiceNameForKey(m_serviceDisplayName);
    loadSettings(); // Load settings upon construction

    connect(&m_generateWatcher, &QFutureWatcher<QVariantMap>::finished, 
            this, &OpenAICompatibleService::handleGenerateFinished);
    connect(&m_fetchModelsWatcher, &QFutureWatcher<QPair<QStringList, QString>>::finished, 
            this, &OpenAICompatibleService::handleFetchModelsFinished);
}

QString OpenAICompatibleService::getName() const {
    return m_serviceDisplayName;
}

bool OpenAICompatibleService::isConfigured() {
    // API key might be optional for some local OpenAI-compatible servers
    return !m_baseUrl.isEmpty() && QUrl(m_baseUrl).isValid();
}

QString OpenAICompatibleService::sanitizeServiceNameForKey(const QString& name) const {
    QString key = name;
    // Remove characters not suitable for QSettings keys or group names
    key.remove(QRegularExpression(R"([^a-zA-Z0-9_-])")); 
    return "OpenAIService_" + key; // Prefix to avoid clashes and identify type
}

void OpenAICompatibleService::loadSettings() {
    QSettings settings(QCoreApplication::organizationName(), QCoreApplication::applicationName());
    settings.beginGroup(m_serviceSettingsKey);
    m_baseUrl = settings.value("baseUrl", m_defaultBaseUrl).toString();
    m_apiKey = settings.value("apiKey", "").toString(); // API key can be empty
    m_lastSelectedModel = settings.value("lastSelectedModel", "").toString();
    settings.endGroup();
}

void OpenAICompatibleService::saveSettings() {
    QSettings settings(QCoreApplication::organizationName(), QCoreApplication::applicationName());
    settings.beginGroup(m_serviceSettingsKey);
    settings.setValue("baseUrl", m_baseUrl);
    settings.setValue("apiKey", m_apiKey);
    settings.setValue("lastSelectedModel", m_lastSelectedModel);
    settings.endGroup();
}

void OpenAICompatibleService::configureService(QWidget* parent) {
    m_dialogParent = parent;
    bool ok;

    // Prompt for Base URL
    QString currentBaseUrl = m_baseUrl.isEmpty() ? m_defaultBaseUrl : m_baseUrl;
    QString baseUrlInput = QInputDialog::getText(parent, tr("Configure ") + m_serviceDisplayName,
                                             tr("Base URL (e.g., http://localhost:1234/v1):"), 
                                             QLineEdit::Normal, currentBaseUrl, &ok);
    if (ok && !baseUrlInput.isEmpty()) {
        QUrl qurl(baseUrlInput);
        if (qurl.isValid() && !qurl.host().isEmpty()) {
            m_baseUrl = qurl.toString();
            // Ensure /v1 suffix for OpenAI compatibility if often forgotten
            if (!m_baseUrl.endsWith("/v1") && !m_baseUrl.endsWith("/v1/")) {
                 if (m_baseUrl.endsWith("/")) m_baseUrl.chop(1); // remove trailing / before adding /v1
                 m_baseUrl += "/v1";
                 QMessageBox::information(parent, "URL Adjusted", "Appended '/v1' to the Base URL for OpenAI compatibility.");
            }
            if (m_baseUrl.endsWith("/")) m_baseUrl.chop(1); // Ensure no trailing slash for consistency
            
        } else {
            QMessageBox::warning(parent, "Invalid URL", "The entered URL is not valid. Please include scheme and host.");
        }
    } else if (ok && baseUrlInput.isEmpty()) {
        m_baseUrl.clear(); // User cleared it
    }
    // If user cancelled, m_baseUrl remains as it was.

    // Prompt for API Key (can be optional)
    QString currentApiKey = m_apiKey;
    QString apiKeyInput = QInputDialog::getText(parent, tr("Configure ") + m_serviceDisplayName,
                                            tr("API Key (if required, leave empty if not):"), 
                                            QLineEdit::Normal, // Not password mode, some keys aren't secret or user might want to see
                                            currentApiKey, &ok);
    if (ok) { // User pressed OK, even if input is empty
        m_apiKey = apiKeyInput;
    }
    // If user cancelled, m_apiKey remains as it was.

    saveSettings();

    if (!isConfigured()) { // isConfigured mainly checks base URL
        QMessageBox::warning(parent, "Configuration Potentially Incomplete", 
                             m_serviceDisplayName + " Base URL is missing or invalid. The service may not function.");
    } else {
        QMessageBox::information(parent, "Configuration Saved", m_serviceDisplayName + " settings saved.");
    }
}

void OpenAICompatibleService::fetchModels(QWidget* parent) {
    m_dialogParent = parent;
    if (!isConfigured()) {
        emit modelsFetched(QStringList(), m_serviceDisplayName + " not configured (Base URL missing).");
        return;
    }

    QString currentBaseUrl = m_baseUrl; // Capture for lambda
    QString currentApiKey = m_apiKey;   // Capture for lambda

    QFuture<QPair<QStringList, QString>> future = QtConcurrent::run([currentBaseUrl, currentApiKey]() {
        QStringList modelNames;
        QString errorString;
        // OpenAI-compatible path for models is typically <base_url>/models (base_url already includes /v1 if needed)
        QUrl url(currentBaseUrl + "/models"); 

        try {
            httplib::Client cli(url.host().toStdString().c_str(), url.port(url.scheme().toLower() == "https" ? 443 : 80));
            if (url.scheme().toLower() == "https") {
                cli.enable_server_certificate_verification(true);
            }
            cli.set_connection_timeout(5); 
            cli.set_read_timeout(15);      

            httplib::Headers headers;
            if (!currentApiKey.isEmpty()) {
                headers.emplace("Authorization", "Bearer " + currentApiKey.toStdString());
            }

            auto res = cli.Get(url.path().toStdString().c_str(), headers);

            if (!res) {
                errorString = "HTTP client error: " + httplib::to_string(res.error());
                return QPair<QStringList, QString>(modelNames, errorString);
            }

            if (res->status != 200) {
                errorString = "API Error: HTTP " + QString::number(res->status) + ". " + QString::fromStdString(res->body);
                return QPair<QStringList, QString>(modelNames, errorString);
            }

            nlohmann::json jsonResponse = nlohmann::json::parse(res->body);
            // Standard OpenAI response for /v1/models is a list with a "data" key
            // Each item in "data" is an object with an "id" field for the model name
            if (jsonResponse.contains("data") && jsonResponse["data"].is_array()) {
                for (const auto& model_obj : jsonResponse["data"]) {
                    if (model_obj.contains("id") && model_obj["id"].is_string()) {
                        modelNames.append(QString::fromStdString(model_obj["id"].get<std::string>()));
                    }
                }
            } else {
                errorString = "Invalid JSON response: 'data' array not found for models list.";
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
        
        modelNames.sort(); // Changed from qSort to modelNames.sort() for QStringList
        return QPair<QStringList, QString>(modelNames, errorString);
    });
    m_fetchModelsWatcher.setFuture(future);
}

void OpenAICompatibleService::handleFetchModelsFinished() {
    QPair<QStringList, QString> result = m_fetchModelsWatcher.result();
    emit modelsFetched(result.first, result.second);
}

void OpenAICompatibleService::generateResponse(const std::vector<Common::ConversationTurn>& history, 
                                               const QString& promptText, // latest prompt
                                               QWidget* parentWidget) {
    m_dialogParent = parentWidget;
    if (!isConfigured()) {
        emit responseReady("", "", "", m_serviceDisplayName + " not configured (Base URL missing).");
        return;
    }
    if (getLastSelectedModel().isEmpty()){
        emit responseReady("", "", "", "No model selected for " + m_serviceDisplayName + ".");
        return;
    }

    QString currentBaseUrl = m_baseUrl;
    QString currentApiKey = m_apiKey;
    QString selectedModel = getLastSelectedModel();
    std::vector<Common::ConversationTurn> historyCopy = history; // Includes latest user prompt

    QFuture<QVariantMap> future = QtConcurrent::run([currentBaseUrl, currentApiKey, selectedModel, historyCopy]() {
        QVariantMap resultData;
        nlohmann::json request_payload_json;
        
        try {
            nlohmann::json messages_json = nlohmann::json::array();
            for (const auto& turn : historyCopy) {
                std::string role = turn.role;
                // OpenAI uses "user", "assistant", "system".
                if (role == "model") role = "assistant"; 
                // TODO: Consider how to handle "system" prompts if Common::ConversationTurn supports them.
                // For now, assuming "user" and "assistant" are primary.
                messages_json.push_back({{"role", role}, {"content", turn.text}});
            }

            request_payload_json["model"] = selectedModel.toStdString();
            request_payload_json["messages"] = messages_json;
            // request_payload_json["stream"] = false; // Not explicitly set, defaults to false for OpenAI

            std::string request_body_str = request_payload_json.dump();
            resultData["requestPayload"] = QString::fromStdString(request_body_str);

            QUrl url(currentBaseUrl + "/chat/completions"); // Standard OpenAI path
            httplib::Client cli(url.host().toStdString().c_str(), url.port(url.scheme().toLower() == "https" ? 443 : 80));
            if (url.scheme().toLower() == "https") {
                cli.enable_server_certificate_verification(true);
            }
            cli.set_connection_timeout(10); 
            cli.set_read_timeout(120);    

            httplib::Headers headers = { {"Content-Type", "application/json"} };
            if (!currentApiKey.isEmpty()) {
                headers.emplace("Authorization", "Bearer " + currentApiKey.toStdString());
            }

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
            // Standard OpenAI response: choices[0].message.content
            if (response_json.contains("choices") && response_json["choices"].is_array() && !response_json["choices"].empty()) {
                const auto& first_choice = response_json["choices"][0];
                if (first_choice.contains("message") && first_choice["message"].is_object()) {
                    const auto& message_obj = first_choice["message"];
                    if (message_obj.contains("content") && message_obj["content"].is_string()) {
                        resultData["responseText"] = QString::fromStdString(message_obj["content"].get<std::string>());
                    } else { resultData["errorString"] = "API Response: 'choices[0].message.content' missing or not string."; }
                } else { resultData["errorString"] = "API Response: 'choices[0].message' missing or not object."; }
            } else { resultData["errorString"] = "API Response: 'choices' array missing or empty."; }

        } catch (const nlohmann::json::parse_error& e) {
            resultData["errorString"] = "JSON Parse Error (OpenAI Response): " + QString::fromUtf8(e.what());
        } catch (const nlohmann::json::exception& e) { 
            resultData["errorString"] = "JSON Data Error (OpenAI Response): " + QString::fromUtf8(e.what());
        } catch (const std::exception& e) {
            resultData["errorString"] = "Standard Exception (OpenAI Call): " + QString::fromUtf8(e.what());
        } catch (...) {
            resultData["errorString"] = "Unknown error in OpenAI API call thread.";
        }
        return resultData;
    });

    m_generateWatcher.setFuture(future);
}

void OpenAICompatibleService::handleGenerateFinished() {
    QVariantMap result = m_generateWatcher.result();
    emit responseReady(
        result.value("responseText").toString(),
        result.value("requestPayload").toString(),
        result.value("fullResponsePayload").toString(),
        result.value("errorString").toString()
    );
}
