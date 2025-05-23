// openai_compatible_service.cpp

#include "openai_compatible_service.h"
#include "mainwindow.h" // For ConversationTurn struct (ensure this is accessible)

#include <QInputDialog>
#include <QMessageBox>
#include <QSettings>
#include <QUrl>
#include <QtConcurrent/QtConcurrent> // For QtConcurrent::run
#include <QRegularExpression>      // For sanitizing service name for settings key
#include <QDebug>                  // For qInfo, qWarning

// If still using httplib for consistency
#define CPPHTTPLIB_OPENSSL_SUPPORT // Only if your server is HTTPS
#include "httplib.h"               // From vendor
#include "nlohmann/json.hpp"       // From vendor

using json = nlohmann::json;

// ns_turn for ConversationTurn JSON conversion
namespace ns_turn {
    void to_json(json& j, const ConversationTurn& t) {
        // OpenAI API expects "assistant" for the model's role
        std::string role = t.role;
        if (role == "model") {
            role = "assistant";
        }
        j = json{
            {"role", role},
            {"content", t.text}
        };
    }
}


OpenAICompatibleService::OpenAICompatibleService(const QString& serviceDisplayName,
                                                 const QString& defaultBaseUrl,
                                                 QObject* parent)
    : LLMService(parent),
      m_serviceDisplayName(serviceDisplayName),
      m_defaultBaseUrl(defaultBaseUrl) {

    QSettings settings("MyCompany", "UniversalLLMClient");
    QString settingsKeyBase = getSettingsKeyBase();

    m_baseUrl = settings.value(settingsKeyBase + "/baseUrl", m_defaultBaseUrl).toString();
    m_lastSelectedModel = settings.value(settingsKeyBase + "/lastModel").toString();

    connect(&m_modelFetchWatcher, &QFutureWatcher<QPair<QStringList, QString>>::finished,
            this, &OpenAICompatibleService::handleModelFetchResponse);
    connect(&m_generateWatcher, &QFutureWatcher<QVariantMap>::finished,
            this, &OpenAICompatibleService::handleGenerateResponse);
}

QString OpenAICompatibleService::getName() const {
    return m_serviceDisplayName;
}

// Helper to create a sanitized settings key from the service display name
QString OpenAICompatibleService::getSettingsKeyBase() const {
    QString key = m_serviceDisplayName;
    key.remove(QRegularExpression("[^a-zA-Z0-9_]")); // Remove non-alphanumeric characters
    return key;
}


bool OpenAICompatibleService::isConfigured() {
    return !m_baseUrl.isEmpty();
}

bool OpenAICompatibleService::configureService(QWidget* parentWindow) {
    bool ok;
    QString dialogTitle = QString("Configure %1").arg(m_serviceDisplayName);
    QString labelText = QString("%1 Base URL (e.g., %2/v1):").arg(m_serviceDisplayName, m_defaultBaseUrl.section('/', 0, -2)); // Show example without /v1 initially

    QString currentUrlToEdit = m_baseUrl;
    // if (currentUrlToEdit.endsWith("/v1")) { // Common to forget to add /v1
    //     currentUrlToEdit.chop(3);
    // }


    QString newBaseUrlInput = QInputDialog::getText(parentWindow,
                                                 dialogTitle,
                                                 labelText,
                                                 QLineEdit::Normal,
                                                 currentUrlToEdit, // Show current URL without /v1 if it ends with it
                                                 &ok);
    if (ok) {
        QString newBaseUrl = newBaseUrlInput.trimmed();
        if (newBaseUrl.isEmpty()) {
             QMessageBox::warning(parentWindow, "Configuration Error", QString("%1 Base URL cannot be empty.").arg(m_serviceDisplayName));
            return false;
        }
        // Ensure it ends with /v1 if not already present, as this is common for OpenAI-compat APIs
        if (!newBaseUrl.endsWith("/v1", Qt::CaseInsensitive)) {
            if (newBaseUrl.endsWith("/")) newBaseUrl.chop(1);
            newBaseUrl += "/v1";
        }


        QUrl testUrl(newBaseUrl);
        if (testUrl.isValid() && (testUrl.scheme() == "http" || testUrl.scheme() == "https")) {
            m_baseUrl = newBaseUrl;
            QSettings settings("MyCompany", "UniversalLLMClient");
            settings.setValue(getSettingsKeyBase() + "/baseUrl", m_baseUrl);
            qInfo() << m_serviceDisplayName << "base URL configured:" << m_baseUrl;
            return true;
        } else {
            QMessageBox::warning(parentWindow, "Configuration Error", QString("Invalid %1 Base URL. It must be a valid HTTP/HTTPS URL ending with /v1 (e.g., http://localhost:1234/v1).").arg(m_serviceDisplayName));
            return false;
        }
    }
    return false; // User cancelled
}

void OpenAICompatibleService::fetchModels(std::function<void(QStringList, QString)> callback) {
    m_modelFetchCallback = callback;
    if (!isConfigured()) {
        if (m_modelFetchCallback) m_modelFetchCallback({}, QString("%1 base URL not configured.").arg(m_serviceDisplayName));
        m_modelFetchCallback = nullptr;
        return;
    }

    QFuture<QPair<QStringList, QString>> future = QtConcurrent::run([this]() -> QPair<QStringList, QString> {
        QUrl url(m_baseUrl + "/models"); // OpenAI-compatible endpoint for listing models
        if (!url.isValid()) {
            return qMakePair(QStringList(), QString("Invalid Base URL for %1: %2").arg(m_serviceDisplayName, m_baseUrl));
        }
        QString host = url.host();
        int port = url.port(url.scheme() == "https" ? 443 : 80);

        httplib::Client cli(host.toStdString(), port);
        if (url.scheme() == "https") {
            #ifdef CPPHTTPLIB_OPENSSL_SUPPORT
                // httplib SSL enabled
            #else
                return qMakePair(QStringList(), QString("Error: %1 URL is HTTPS but SSL support not compiled in httplib.").arg(m_serviceDisplayName));
            #endif
        }
        cli.set_connection_timeout(5, 0);

        auto res = cli.Get(url.path().toStdString().c_str()); // Path is typically "/v1/models"

        if (!res) {
            return qMakePair(QStringList(), QString("Error: HTTP request to %1 models endpoint failed - %2").arg(m_serviceDisplayName, httplib::to_string(res.error()).c_str()));
        }

        if (res->status != 200) {
            return qMakePair(QStringList(), QString("Error: %1 API for models responded with status %2. Body: %3").arg(m_serviceDisplayName).arg(res->status).arg(QString::fromStdString(res->body)));
        }

        QStringList modelNames;
        try {
            json j = json::parse(res->body);
            if (j.contains("data") && j["data"].is_array()) {
                for (const auto& model_item : j["data"]) {
                    if (model_item.contains("id") && model_item["id"].is_string()) {
                        modelNames.append(QString::fromStdString(model_item["id"].get<std::string>()));
                    }
                }
            } else {
                 return qMakePair(QStringList(), QString("Error: Unexpected JSON structure for %1 models response.").arg(m_serviceDisplayName));
            }
            return qMakePair(modelNames, QString()); // Empty error string for success
        } catch (const json::parse_error& e) {
            return qMakePair(QStringList(), QString("Error: Failed to parse %1 models JSON response - %2. Body: %3").arg(m_serviceDisplayName, e.what()).arg(QString::fromStdString(res->body)));
        } catch (const std::exception& e) {
            return qMakePair(QStringList(), QString("Error: Exception while processing %1 models response - %2").arg(m_serviceDisplayName, e.what()));
        }
    });
    m_modelFetchWatcher.setFuture(future);
}

void OpenAICompatibleService::handleModelFetchResponse() {
    if (!m_modelFetchCallback) return;
    QPair<QStringList, QString> result = m_modelFetchWatcher.future().result();
    m_modelFetchCallback(result.first, result.second);
    m_modelFetchCallback = nullptr;
}

void OpenAICompatibleService::generateResponse(
    const std::vector<ConversationTurn>& history,
    const QString& systemPrompt,
    const QString& modelName,
    std::function<void(QString, QString, QString, QString)> callback)
{
    m_generateCallback = callback;
    if (!isConfigured() || modelName.isEmpty()) {
        if (m_generateCallback) m_generateCallback("", "", "", QString("%1 not configured or no model selected.").arg(m_serviceDisplayName));
        m_generateCallback = nullptr;
        return;
    }

    QFuture<QVariantMap> future = QtConcurrent::run([this, history, systemPrompt, modelName]() -> QVariantMap {
        QVariantMap resultOutput;
        QUrl url(m_baseUrl); // Base URL should already end with /v1
        if (!url.isValid()) {
            resultOutput["error"] = QString("Invalid Base URL for %1: %2").arg(m_serviceDisplayName, m_baseUrl);
            return resultOutput;
        }
        QString host = url.host();
        int port = url.port(url.scheme() == "https" ? 443 : 80);

        httplib::Client cli(host.toStdString(), port);
        if (url.scheme() == "https") {
            #ifdef CPPHTTPLIB_OPENSSL_SUPPORT
                // httplib SSL enabled
            #else
                resultOutput["error"] = QString("Error: %1 URL is HTTPS but SSL support not compiled in httplib.").arg(m_serviceDisplayName);
                return resultOutput;
            #endif
        }
        cli.set_connection_timeout(10, 0);
        cli.set_read_timeout(300, 0); // 5 minutes

        json request_body_json;
        request_body_json["model"] = modelName.toStdString();
        request_body_json["stream"] = false; // Not handling streaming responses

        json messages_json = json::array();
        if (!systemPrompt.isEmpty()) {
            messages_json.push_back({{"role", "system"}, {"content", systemPrompt.toStdString()}});
        }
        for (const auto& turn : history) {
            messages_json.push_back(ns_turn::ConversationTurn{turn.role, turn.text}); // ns_turn maps "model" to "assistant"
        }
        request_body_json["messages"] = messages_json;

        // Optional: Add temperature, max_tokens, etc.
        // request_body_json["temperature"] = 0.7;
        // request_body_json["max_tokens"] = 2048;

        std::string requestPayloadStr = request_body_json.dump(2);
        resultOutput["request_payload"] = QString::fromStdString(requestPayloadStr);

        // Path for chat completions is typically "/chat/completions" relative to base_url (which includes /v1)
        // So, if m_baseUrl = http://localhost:1234/v1, then path is "/chat/completions"
        auto res = cli.Post("/chat/completions", requestPayloadStr, "application/json");

        if (!res) {
            resultOutput["error"] = QString("Error: HTTP request to %1 /chat/completions failed - %2").arg(m_serviceDisplayName, httplib::to_string(res.error()).c_str());
            return resultOutput;
        }

        QString fullResponsePayloadStr = QString::fromStdString(res->body);
        resultOutput["full_response_payload"] = fullResponsePayloadStr;

        if (res->status != 200) {
            resultOutput["error"] = QString("Error: %1 API /chat/completions responded with status %2. Body: %3").arg(m_serviceDisplayName).arg(res->status).arg(fullResponsePayloadStr);
            return resultOutput;
        }

        try {
            json j_res = json::parse(res->body);
            if (j_res.contains("choices") && j_res["choices"].is_array() && !j_res["choices"].empty() &&
                j_res["choices"][0].is_object() &&
                j_res["choices"][0].contains("message") && j_res["choices"][0]["message"].is_object() &&
                j_res["choices"][0]["message"].contains("content") &&
                j_res["choices"][0]["message"]["content"].is_string()) {
                resultOutput["response_text"] = QString::fromStdString(j_res["choices"][0]["message"]["content"].get<std::string>());
            } else if (j_res.contains("error") && j_res["error"].is_object() && j_res["error"].contains("message")) {
                 resultOutput["error"] = QString("%1 API Error: %2").arg(m_serviceDisplayName, QString::fromStdString(j_res["error"]["message"].get<std::string>()));
            }
            else {
                resultOutput["error"] = QString("Error: Could not parse 'choices[0].message.content' from %1 response. Unexpected JSON structure.").arg(m_serviceDisplayName);
            }
        } catch (const json::parse_error& e) {
            resultOutput["error"] = QString("Error: Failed to parse %1 JSON response - %2. Body: %3").arg(m_serviceDisplayName, e.what()).arg(fullResponsePayloadStr);
        } catch (const std::exception& e) {
            resultOutput["error"] = QString("Error: Exception while processing %1 response - %2").arg(m_serviceDisplayName, e.what());
        }
        return resultOutput;
    });
    m_generateWatcher.setFuture(future);
}

void OpenAICompatibleService::handleGenerateResponse() {
    if (!m_generateCallback) return;

    QVariantMap resultData = m_generateWatcher.future().result();
    QString error = resultData.value("error").toString();
    QString requestPayload = resultData.value("request_payload").toString();
    QString fullResponsePayload = resultData.value("full_response_payload").toString();
    QString responseText = resultData.value("response_text").toString();

    if (!error.isEmpty()) {
        m_generateCallback("", requestPayload, fullResponsePayload, error);
    } else {
        m_generateCallback(responseText, requestPayload, fullResponsePayload, "");
    }
    m_generateCallback = nullptr;
}