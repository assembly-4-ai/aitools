// ollama_service.cpp

#include "ollama_service.h"
#include "mainwindow.h" // For ConversationTurn struct (ensure this is accessible or move ConversationTurn)
                        // If ConversationTurn is simple enough, you might redefine it or make it a nested struct.

#include <QInputDialog>
#include <QMessageBox>
#include <QSettings>
#include <QUrl>
#include <QNetworkRequest>   // Using Qt's network classes can be an alternative to httplib
#include <QNetworkAccessManager> // for more Qt-idiomatic async, but httplib is fine.
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QtConcurrent/QtConcurrent> // For QtConcurrent::run
#include <QDebug>                  // For qInfo, qWarning

// If still using httplib for consistency with other services:
#define CPPHTTPLIB_OPENSSL_SUPPORT // Only if your Ollama is HTTPS, usually it's HTTP for local
#include "httplib.h"               // From vendor
#include "nlohmann/json.hpp"       // From vendor

// Assuming nlohmann::json is used if httplib is used, otherwise Qt's JSON classes.
using json = nlohmann::json;

// If ConversationTurn is in mainwindow.h and mainwindow.h includes nlohmann/json
// and defines ns_turn, ensure that's appropriately handled.
// For simplicity, let's assume ConversationTurn is defined and ns_turn helpers are available
// if nlohmann::json is the primary JSON lib.
namespace ns_turn { // Make sure this is consistent with where ConversationTurn is fully defined
    void to_json(json& j, const ConversationTurn& t) {
        j = json{
            {"role", t.role == "model" ? "assistant" : t.role}, // Ollama expects "assistant"
            {"content", t.text}
        };
    }
    // from_json might not be needed in this service if we only construct messages
}


OllamaService::OllamaService(QObject* parent)
    : LLMService(parent) {
    QSettings settings("MyCompany", "UniversalLLMClient"); // Consistent app name
    m_baseUrl = settings.value("Ollama/baseUrl", "http://localhost:11434").toString();
    m_lastSelectedModel = settings.value("Ollama/lastModel").toString();

    connect(&m_modelFetchWatcher, &QFutureWatcher<QPair<QStringList, QString>>::finished,
            this, &OllamaService::handleModelFetchResponse);
    connect(&m_generateWatcher, &QFutureWatcher<QVariantMap>::finished,
            this, &OllamaService::handleGenerateResponse);
}

QString OllamaService::getName() const {
    return "Ollama";
}

bool OllamaService::isConfigured() {
    return !m_baseUrl.isEmpty();
}

bool OllamaService::configureService(QWidget* parentWindow) {
    bool ok;
    QString newBaseUrl = QInputDialog::getText(parentWindow,
                                               "Configure Ollama",
                                               "Ollama Base URL (e.g., http://localhost:11434):",
                                               QLineEdit::Normal,
                                               m_baseUrl,
                                               &ok);
    if (ok) {
        QUrl testUrl(newBaseUrl);
        if (testUrl.isValid() && !newBaseUrl.isEmpty() && (testUrl.scheme() == "http" || testUrl.scheme() == "https")) {
            m_baseUrl = newBaseUrl;
            QSettings settings("MyCompany", "UniversalLLMClient");
            settings.setValue("Ollama/baseUrl", m_baseUrl);
            qInfo() << "Ollama base URL configured:" << m_baseUrl;
            return true;
        } else {
            QMessageBox::warning(parentWindow, "Configuration Error", "Invalid Ollama Base URL. It must be a valid HTTP/HTTPS URL.");
            return false;
        }
    }
    return false; // User cancelled
}

void OllamaService::fetchModels(std::function<void(QStringList, QString)> callback) {
    m_modelFetchCallback = callback;
    if (!isConfigured()) {
        if (m_modelFetchCallback) m_modelFetchCallback({}, "Ollama base URL not configured.");
        m_modelFetchCallback = nullptr;
        return;
    }

    QFuture<QPair<QStringList, QString>> future = QtConcurrent::run([this]() -> QPair<QStringList, QString> {
        QUrl url(m_baseUrl);
        if (!url.isValid()) {
            return qMakePair(QStringList(), QString("Invalid Ollama Base URL: %1").arg(m_baseUrl));
        }
        QString host = url.host();
        int port = url.port(url.scheme() == "https" ? 443 : 80); // Default port based on scheme

        httplib::Client cli(host.toStdString(), port);
        if (url.scheme() == "https") {
            #ifdef CPPHTTPLIB_OPENSSL_SUPPORT
                // httplib will use SSL if CPPHTTPLIB_OPENSSL_SUPPORT is defined and linked
            #else
                return qMakePair(QStringList(), "Error: Ollama URL is HTTPS but SSL support not compiled in httplib.");
            #endif
        }
        cli.set_connection_timeout(5, 0); // 5 seconds

        auto res = cli.Get("/api/tags"); // Ollama endpoint for installed models

        if (!res) {
            return qMakePair(QStringList(), QString("Error: HTTP request to Ollama failed - %1").arg(httplib::to_string(res.error()).c_str()));
        }

        if (res->status != 200) {
            return qMakePair(QStringList(), QString("Error: Ollama API for models responded with status %1. Body: %2").arg(res->status).arg(QString::fromStdString(res->body)));
        }

        QStringList modelNames;
        try {
            json j = json::parse(res->body);
            if (j.contains("models") && j["models"].is_array()) {
                for (const auto& model_item : j["models"]) {
                    if (model_item.contains("name") && model_item["name"].is_string()) {
                        modelNames.append(QString::fromStdString(model_item["name"].get<std::string>()));
                    }
                }
            } else {
                 return qMakePair(QStringList(), "Error: Unexpected JSON structure for Ollama models response.");
            }
            return qMakePair(modelNames, QString()); // Empty error string for success
        } catch (const json::parse_error& e) {
            return qMakePair(QStringList(), QString("Error: Failed to parse Ollama models JSON response - %1. Body: %2").arg(e.what()).arg(QString::fromStdString(res->body)));
        } catch (const std::exception& e) {
            return qMakePair(QStringList(), QString("Error: Exception while processing Ollama models response - %1").arg(e.what()));
        }
    });
    m_modelFetchWatcher.setFuture(future);
}

void OllamaService::handleModelFetchResponse() {
    if (!m_modelFetchCallback) return;
    QPair<QStringList, QString> result = m_modelFetchWatcher.future().result();
    m_modelFetchCallback(result.first, result.second);
    m_modelFetchCallback = nullptr; // Clear callback after use
}

void OllamaService::generateResponse(
    const std::vector<ConversationTurn>& history,
    const QString& systemPrompt,
    const QString& modelName,
    std::function<void(QString /*response_text*/, QString /*request_payload*/, QString /*full_response_payload*/, QString /*error*/)> callback)
{
    m_generateCallback = callback;
    if (!isConfigured() || modelName.isEmpty()) {
        if (m_generateCallback) m_generateCallback("", "", "", "Ollama not configured or no model selected.");
        m_generateCallback = nullptr;
        return;
    }

    QFuture<QVariantMap> future = QtConcurrent::run([this, history, systemPrompt, modelName]() -> QVariantMap {
        QVariantMap resultOutput;
        QUrl url(m_baseUrl);
         if (!url.isValid()) {
            resultOutput["error"] = QString("Invalid Ollama Base URL: %1").arg(m_baseUrl);
            return resultOutput;
        }
        QString host = url.host();
        int port = url.port(url.scheme() == "https" ? 443 : 80);

        httplib::Client cli(host.toStdString(), port);
         if (url.scheme() == "https") {
            #ifdef CPPHTTPLIB_OPENSSL_SUPPORT
                // httplib SSL enabled
            #else
                resultOutput["error"] = "Error: Ollama URL is HTTPS but SSL support not compiled in httplib.";
                return resultOutput;
            #endif
        }
        cli.set_connection_timeout(10, 0);      // Connection timeout
        cli.set_read_timeout(300, 0);           // 5 minutes read timeout for generation

        json request_body_json;
        request_body_json["model"] = modelName.toStdString();
        request_body_json["stream"] = false; // We want the full response, not streaming

        json messages_json = json::array();
        if (!systemPrompt.isEmpty()) {
            messages_json.push_back({{"role", "system"}, {"content", systemPrompt.toStdString()}});
        }
        for (const auto& turn : history) {
            messages_json.push_back(ns_turn::ConversationTurn{turn.role, turn.text}); // Uses to_json for role mapping
        }
        request_body_json["messages"] = messages_json;

        // Options (can be made configurable later)
        // request_body_json["options"] = {
        //     {"temperature", 0.7}
        // };

        std::string requestPayloadStr = request_body_json.dump(2); // Pretty print for logging
        resultOutput["request_payload"] = QString::fromStdString(requestPayloadStr);

        auto res = cli.Post("/api/chat", requestPayloadStr, "application/json");

        if (!res) {
            resultOutput["error"] = QString("Error: HTTP request to Ollama /api/chat failed - %1").arg(httplib::to_string(res.error()).c_str());
            return resultOutput;
        }

        QString fullResponsePayloadStr = QString::fromStdString(res->body);
        resultOutput["full_response_payload"] = fullResponsePayloadStr;

        if (res->status != 200) {
            resultOutput["error"] = QString("Error: Ollama API /api/chat responded with status %1. Body: %2").arg(res->status).arg(fullResponsePayloadStr);
            return resultOutput;
        }

        try {
            json j_res = json::parse(res->body);
            if (j_res.contains("message") && j_res["message"].is_object() &&
                j_res["message"].contains("content") && j_res["message"]["content"].is_string()) {
                resultOutput["response_text"] = QString::fromStdString(j_res["message"]["content"].get<std::string>());
            } else if (j_res.contains("error") && j_res["error"].is_string()) { // Ollama might return error in JSON body
                 resultOutput["error"] = QString("Ollama API Error: %1").arg(QString::fromStdString(j_res["error"].get<std::string>()));
            }
            else {
                resultOutput["error"] = "Error: Could not parse 'message.content' from Ollama response. Unexpected JSON structure.";
            }
        } catch (const json::parse_error& e) {
            resultOutput["error"] = QString("Error: Failed to parse Ollama JSON response - %1. Body: %2").arg(e.what()).arg(fullResponsePayloadStr);
        } catch (const std::exception& e) {
            resultOutput["error"] = QString("Error: Exception while processing Ollama response - %1").arg(e.what());
        }
        return resultOutput;
    });
    m_generateWatcher.setFuture(future);
}

void OllamaService::handleGenerateResponse() {
    if (!m_generateCallback) return;

    QVariantMap resultData = m_generateWatcher.future().result();
    QString error = resultData.value("error").toString();
    QString requestPayload = resultData.value("request_payload").toString();
    QString fullResponsePayload = resultData.value("full_response_payload").toString();
    QString responseText = resultData.value("response_text").toString();

    if (!error.isEmpty()) {
        m_generateCallback("", requestPayload, fullResponsePayload, error); // Pass empty response text on error
    } else {
        m_generateCallback(responseText, requestPayload, fullResponsePayload, ""); // Empty error string for success
    }
    m_generateCallback = nullptr; // Clear callback
}