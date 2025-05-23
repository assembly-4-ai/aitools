#include "llm_services/gemini_service.h" // Adjusted
#include "ui/mainwindow.h" // For ConversationTurn (ideally move ConversationTurn to a common place)
#include <QInputDialog>
#include <QMessageBox>
#include <QSettings>
#include <QtConcurrent/QtConcurrent>
#include <QCoreApplication> // For QCoreApplication::applicationDirPath() if needed

#define CPPHTTPLIB_OPENSSL_SUPPORT
#include "vendor/httplib.h"
#include "vendor/nlohmann/json.hpp"

using json = nlohmann::json;

// Assuming ConversationTurn is defined in mainwindow.h or similar
// and ns_turn for JSON conversion for ConversationTurn is available
namespace ns_turn {
    void to_json(json& j, const ConversationTurn& t) {
        j = json{
            {"role", t.role},
            {"parts", json::array({{{"text", t.text}}})}
        };
    }
    // from_json might not be needed by this service if it only sends
}


const std::string GEMINI_API_BASE_URL_GS = "generativelanguage.googleapis.com";

GeminiService::GeminiService(QObject* parent) : LLMService(parent) {
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, "MyCompany", "UniversalLLMClient");
    m_apiKey = settings.value("Gemini/apiKey").toString();
    m_modelIdentifier = settings.value("Gemini/modelIdentifier", "gemini-pro").toString();
    m_lastSelectedModel = m_modelIdentifier;

    connect(&m_httpWatcher, &QFutureWatcher<QVariantMap>::finished, this, &GeminiService::handleHttpResponse);
}

bool GeminiService::isConfigured() {
    return !m_apiKey.isEmpty() && !m_modelIdentifier.isEmpty();
}

bool GeminiService::configureService(QWidget* parentWindow) {
    bool ok;
    QString currentApiKey = m_apiKey;
    QString newApiKey = QInputDialog::getText(parentWindow, "Configure Gemini", "Gemini API Key:", QLineEdit::Normal, currentApiKey, &ok);
    if (ok) {
        if (newApiKey.isEmpty()) {
            QMessageBox::warning(parentWindow, "Configuration Error", "Gemini API Key cannot be empty if you proceed.");
            // Allow clearing if they confirm, or just return false
            m_apiKey = newApiKey; // Update even if empty, if they clicked OK
        } else {
            m_apiKey = newApiKey;
        }

        QStringList knownModels = {"gemini-pro", "gemini-1.5-flash-latest", "gemini-1.0-pro", "gemini-1.5-pro-latest"};
        int currentModelIdx = knownModels.indexOf(m_modelIdentifier);
        if (currentModelIdx < 0) currentModelIdx = 0; // Default to first if not found

        QString newModelId = QInputDialog::getItem(parentWindow, "Configure Gemini Model",
                                                   "Select Gemini Model:", knownModels,
                                                   currentModelIdx, false, &ok);
        if (ok && !newModelId.isEmpty()) {
            m_modelIdentifier = newModelId;
            m_lastSelectedModel = m_modelIdentifier; // Update selection
        }

        QSettings settings(QSettings::IniFormat, QSettings::UserScope, "MyCompany", "UniversalLLMClient");
        settings.setValue("Gemini/apiKey", m_apiKey);
        settings.setValue("Gemini/modelIdentifier", m_modelIdentifier);
        return true; // Configuration attempt was made
    }
    return false; // User cancelled
}

void GeminiService::fetchModels(std::function<void(QStringList, QString)> callback) {
    // For Gemini, the model is typically part of the configuration.
    // We return the currently configured model as the only "available" one for this service.
    if (!m_modelIdentifier.isEmpty()) {
        callback({m_modelIdentifier}, "");
    } else {
        callback({}, "Gemini model identifier not configured.");
    }
}

void GeminiService::generateResponse(
    const std::vector<ConversationTurn>& history,
    const QString& systemPrompt,
    const QString& modelName, // Should match m_modelIdentifier
    std::function<void(QString, QString, QString, QString)> callback)
{
    m_generateCallback = callback;

    if (m_apiKey.isEmpty()) {
        if (m_generateCallback) m_generateCallback("", "", "", "Error: Gemini API key is missing.");
        m_generateCallback = nullptr;
        return;
    }
     if (modelName.isEmpty() || modelName != m_modelIdentifier) {
        if (m_generateCallback) m_generateCallback("", "", "", QString("Error: Invalid or mismatched Gemini model. Configured: %1, Requested: %2").arg(m_modelIdentifier, modelName));
        m_generateCallback = nullptr;
        return;
    }


    QFuture<QVariantMap> future = QtConcurrent::run([this, history, systemPrompt, modelName]() -> QVariantMap {
        QVariantMap resultPayload;
        httplib::Client cli(GEMINI_API_BASE_URL_GS.c_str(), 443);
        cli.enable_server_certificate_verification(true);
        cli.set_connection_timeout(20); // 20 seconds
        cli.set_read_timeout(120);    // 120 seconds

        json request_body_json;
        json contents_array = json::array();

        if (!systemPrompt.isEmpty()) {
            // Gemini typically prefers system instructions as the first part of the first user turn,
            // or if the API evolves, a dedicated system_instruction field.
            // For now, prepend to history if history is empty, or combine with first user turn.
            if (history.empty() || history.front().role != "user") {
                 contents_array.push_back(ns_turn::ConversationTurn{"user", systemPrompt.toStdString()});
            }
        }

        for (const auto& turn : history) {
            if (turn.role == "user" && !systemPrompt.isEmpty() && &turn == &history.front() && contents_array.empty()) {
                // If system prompt was not added and this is the first user turn, prepend it
                contents_array.push_back(ns_turn::ConversationTurn{"user", (systemPrompt + "\n\n" + QString::fromStdString(turn.text)).toStdString()});
            } else {
                contents_array.push_back(ns_turn::ConversationTurn{turn.role, turn.text});
            }
        }

        request_body_json["contents"] = contents_array;
        request_body_json["generationConfig"] = {
            {"temperature", 0.7},
            {"maxOutputTokens", 8192} // Increased
        };
        json safetySettingsArray = json::array();
        std::vector<std::pair<std::string, std::string>> safetyCats = {
            {"HARM_CATEGORY_HARASSMENT", "BLOCK_NONE"},
            {"HARM_CATEGORY_HATE_SPEECH", "BLOCK_NONE"},
            {"HARM_CATEGORY_SEXUALLY_EXPLICIT", "BLOCK_NONE"},
            {"HARM_CATEGORY_DANGEROUS_CONTENT", "BLOCK_NONE"}
        };
        for(const auto& cat : safetyCats){
            safetySettingsArray.push_back({{"category", cat.first}, {"threshold", cat.second}});
        }
        request_body_json["safetySettings"] = safetySettingsArray;


        std::string request_payload_str = request_body_json.dump(2);
        resultPayload["request_payload"] = QString::fromStdString(request_payload_str);

        std::string api_path = "/v1beta/models/" + modelName.toStdString() + ":generateContent?key=" + m_apiKey.toStdString();
        httplib::Headers headers = { {"Content-Type", "application/json"} };

        auto res = cli.Post(api_path.c_str(), headers, request_body_json.dump(), "application/json");

        if (!res) {
            resultPayload["error"] = QString("Error: HTTP request failed - %1").arg(httplib::to_string(res.error()).c_str());
            return resultPayload;
        }

        QString full_response_payload_str = QString::fromStdString(res->body);
        resultPayload["full_response_payload"] = full_response_payload_str;

        if (res->status != 200) {
            try {
                json error_json = json::parse(res->body);
                if (error_json.contains("error") && error_json["error"].contains("message")) {
                    resultPayload["error"] = QString("Error: API responded with status %1 - Message: %2")
                                           .arg(res->status)
                                           .arg(QString::fromStdString(error_json["error"]["message"].get<std::string>()));
                    return resultPayload;
                }
            } catch (const json::parse_error&) { /* Not a JSON error */ }
            resultPayload["error"] = QString("Error: API responded with status %1. Body: %2").arg(res->status).arg(full_response_payload_str);
            return resultPayload;
        }

        try {
            json response_json = json::parse(res->body);
            if (response_json.contains("candidates") &&
                response_json["candidates"].is_array() &&
                !response_json["candidates"].empty() &&
                response_json["candidates"][0].contains("content") &&
                response_json["candidates"][0]["content"].contains("parts") &&
                response_json["candidates"][0]["content"]["parts"].is_array() &&
                !response_json["candidates"][0]["content"]["parts"].empty() &&
                response_json["candidates"][0]["content"]["parts"][0].contains("text")) {
                resultPayload["response_text"] = QString::fromStdString(response_json["candidates"][0]["content"]["parts"][0]["text"].get<std::string>());
            } else if (response_json.contains("promptFeedback") && response_json["promptFeedback"].contains("blockReason")){
                 resultPayload["error"] = "Error: Prompt blocked by API. Reason: " + QString::fromStdString(response_json["promptFeedback"]["blockReason"].dump());
            }
            else {
                resultPayload["error"] = "Error: Could not parse text from Gemini response. Unexpected format.";
            }
        } catch (const json::parse_error& e) {
            resultPayload["error"] = QString("Error: Failed to parse JSON response from Gemini: %1").arg(e.what());
        } catch (const std::exception& e) {
            resultPayload["error"] = QString("Error: Exception while processing Gemini response: %1").arg(e.what());
        }
        return resultPayload;
    });
    m_httpWatcher.setFuture(future);
}

void GeminiService::handleHttpResponse() {
    if (!m_generateCallback) return;

    QVariantMap resultData = m_httpWatcher.future().result();
    QString error = resultData.value("error").toString();
    QString reqPayload = resultData.value("request_payload").toString();
    QString respText = resultData.value("response_text").toString();
    QString fullRespPayload = resultData.value("full_response_payload").toString();

    if (!error.isEmpty()) {
        m_generateCallback("", reqPayload, fullRespPayload, error); // Pass fullRespPayload even on error
    } else {
        m_generateCallback(respText, reqPayload, fullRespPayload, "");
    }
    m_generateCallback = nullptr;
}