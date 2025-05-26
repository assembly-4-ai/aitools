#define CPPHTTPLIB_OPENSSL_SUPPORT // For httplib SSL
#include "vendor/nlohmann/json.hpp" // For JSON manipulation
#include "llm_services/gemini_service.h"
#include "common/types.h"      // For Common::ConversationTurn

#include <QInputDialog>
#include <QSettings>
#include <QWidget>
#include <QMessageBox>        // For displaying errors from configureService
#include <QtConcurrent/QtConcurrent> // For QtConcurrent::run
#include <QCoreApplication> // Required for QSettings

// httplib.h after its define and nlohmann/json.hpp
#include "vendor/httplib.h"

// Local namespace for Gemini-specific ConversationTurn serialization
// This was previously in mainwindow.cpp, now moved to the service implementation.
namespace ns_turn_gemini {
    void to_json(nlohmann::json& j, const Common::ConversationTurn& t) {
        j = nlohmann::json{
            {"role", t.role}, // Ensure roles are "user" or "model"
            {"parts", nlohmann::json::array({
                nlohmann::json{{"text", t.text}}
            })}
        };
    }
} // namespace ns_turn_gemini

GeminiService::GeminiService(QObject *parent) 
    : LLMService(parent), m_dialogParent(nullptr) {
    loadSettings(); // Load API key and model ID upon construction

    // Connect the internal watcher's finished signal to the internal handler
    connect(&m_apiWatcher, &QFutureWatcher<QVariantMap>::finished, 
            this, &GeminiService::handleApiCallFinished);
}

QString GeminiService::getName() const {
    return "Google Gemini";
}

bool GeminiService::isConfigured() {
    return !m_apiKey.isEmpty() && !m_modelIdentifier.isEmpty();
}

void GeminiService::loadSettings() {
    QSettings settings(QCoreApplication::organizationName(), QCoreApplication::applicationName());
    settings.beginGroup("GeminiService");
    m_apiKey = settings.value("apiKey", "").toString();
    m_modelIdentifier = settings.value("modelId", "gemini-pro").toString(); // Default to gemini-pro
    m_lastSelectedModel = settings.value("lastSelectedModel", m_modelIdentifier).toString();
    settings.endGroup();
}

void GeminiService::saveSettings() {
    QSettings settings(QCoreApplication::organizationName(), QCoreApplication::applicationName());
    settings.beginGroup("GeminiService");
    settings.setValue("apiKey", m_apiKey);
    settings.setValue("modelId", m_modelIdentifier);
    settings.setValue("lastSelectedModel", m_lastSelectedModel); // Save the potentially different last selected model
    settings.endGroup();
}

void GeminiService::configureService(QWidget* parent) {
    m_dialogParent = parent; // Store for potential use in other methods triggered by this config
    bool ok;
    QString currentApiKey = m_apiKey;
    QString currentModelId = m_modelIdentifier;

    // Prompt for API Key
    QString apiKey = QInputDialog::getText(parent, tr("Configure Gemini Service"),
                                           tr("API Key:"), QLineEdit::Password,
                                           currentApiKey, &ok);
    if (ok && !apiKey.isEmpty()) {
        m_apiKey = apiKey;
    } else if (ok && apiKey.isEmpty()) {
        // User entered empty string, potentially to clear it
        QMessageBox::StandardButton reply;
        reply = QMessageBox::question(parent, "Clear API Key?", 
                                      "Are you sure you want to clear the API key? This will disable the service.",
                                      QMessageBox::Yes|QMessageBox::No);
        if (reply == QMessageBox::Yes) {
            m_apiKey.clear();
        }
    } else {
        // User cancelled API key input
        // If key was already empty, we don't need to do anything.
        // If key was not empty and user cancelled, keep existing key.
        if (m_apiKey.isEmpty()) {
             QMessageBox::warning(parent, "Configuration Incomplete", "API Key not set. Gemini service may not function.");
        }
        // Do not proceed to model ID if API key setup was cancelled or resulted in empty.
        saveSettings();
        return;
    }

    // Prompt for Model Identifier (e.g., "gemini-pro")
    // For Gemini, the model is often fixed per API key type or endpoint.
    // For simplicity in Phase 2, we'll allow setting it.
    // A more advanced version might fetch available models if Gemini API supports it.
    QString modelId = QInputDialog::getText(parent, tr("Configure Gemini Service"),
                                            tr("Model Identifier (e.g., gemini-pro):"), QLineEdit::Normal,
                                            currentModelId, &ok);
    if (ok && !modelId.isEmpty()) {
        m_modelIdentifier = modelId;
        if (m_lastSelectedModel.isEmpty() || m_lastSelectedModel == currentModelId) { // If last model was the one we just changed or was empty
            setLastSelectedModel(m_modelIdentifier); // Update last selected model to the new one
        }
    } else if (ok && modelId.isEmpty()) {
        m_modelIdentifier = "gemini-pro"; // Default if cleared
        QMessageBox::information(parent, "Model Identifier", "Model identifier cleared, defaulting to 'gemini-pro'.");
        if (m_lastSelectedModel.isEmpty() || m_lastSelectedModel == currentModelId) {
             setLastSelectedModel(m_modelIdentifier);
        }
    }
    // If user cancelled model ID, keep existing or default.

    saveSettings();
    
    if (!isConfigured()) {
        QMessageBox::warning(parent, "Configuration Incomplete", "Gemini Service is not fully configured (API Key or Model ID missing).");
    } else {
        QMessageBox::information(parent, "Configuration Saved", "Gemini Service settings saved.");
    }
}

void GeminiService::fetchModels(QWidget* parent) {
    m_dialogParent = parent;
    // For Gemini, the model list is not typically fetched dynamically via a separate API call like OpenAI.
    // The model is part of the endpoint or known beforehand (e.g., "gemini-pro", "gemini-pro-vision").
    // For this phase, we will consider the configured m_modelIdentifier as the only "model".
    // If it's configured, emit that. Otherwise, emit an error or empty list.
    if (isConfigured()) {
        // If we want to allow selection between multiple known Gemini models, we could list them here.
        // For now, just return the primary configured model.
        QStringList models = {m_modelIdentifier};
        setLastSelectedModel(m_modelIdentifier); // Ensure this is also set as the current one
        emit modelsFetched(models, QString());
    } else {
        emit modelsFetched(QStringList(), "Service not configured. Please configure API key and model ID.");
    }
}

void GeminiService::generateResponse(const std::vector<Common::ConversationTurn>& history, 
                                     const QString& prompt, // Note: prompt is already part of history if MainWindow adds it
                                     QWidget* parentWidget) {
    m_dialogParent = parentWidget; // Store for potential error dialogs from async callback

    if (!isConfigured()) {
        emit responseReady("", "", "", "Gemini Service not configured. Please set API Key and Model ID.");
        return;
    }

    // The 'prompt' QString is the newest text from the user.
    // The 'history' std::vector<Common::ConversationTurn> is the existing conversation.
    // The Gemini API expects the entire conversation history in the 'contents' field.
    // MainWindow.onSendClicked already adds the user's new prompt to m_conversationHistory before calling this.
    // So, 'history' argument here should be the complete history including the latest prompt.

    std::vector<Common::ConversationTurn> historyCopy = history; // Make a copy for the lambda

    // Capture necessary data by value for the thread
    QString currentApiKey = m_apiKey;
    QString currentModelId = m_modelIdentifier;

    // Use QtConcurrent::run to execute the API call in a separate thread
    QFuture<QVariantMap> future = QtConcurrent::run([currentApiKey, currentModelId, historyCopy]() {
        QVariantMap resultData;
        nlohmann::json request_payload_json;
        std::string host = "generativelanguage.googleapis.com";

        try {
            nlohmann::json contents_json = nlohmann::json::array();
            for (const auto& turn : historyCopy) {
                // Use the custom to_json defined in ns_turn_gemini
                // This ensures roles are "user" or "model" as expected by Gemini
                contents_json.push_back(nlohmann::json::object({
                    {"role", turn.role},
                    {"parts", nlohmann::json::array({
                        nlohmann::json{{"text", turn.text}}
                    })}
                }));
            }
            
            request_payload_json["contents"] = contents_json;
            request_payload_json["generationConfig"] = {
                {"temperature", 0.7}, {"topP", 1.0}, {"maxOutputTokens", 2048} // Increased max tokens
            };
            request_payload_json["safetySettings"] = nlohmann::json::array({
                {{"category", "HARM_CATEGORY_HARASSMENT"}, {"threshold", "BLOCK_MEDIUM_AND_ABOVE"}},
                {{"category", "HARM_CATEGORY_HATE_SPEECH"}, {"threshold", "BLOCK_MEDIUM_AND_ABOVE"}},
                {{"category", "HARM_CATEGORY_SEXUALLY_EXPLICIT"}, {"threshold", "BLOCK_MEDIUM_AND_ABOVE"}},
                {{"category", "HARM_CATEGORY_DANGEROUS_CONTENT"}, {"threshold", "BLOCK_MEDIUM_AND_ABOVE"}}
            });
            
            std::string request_body_str = request_payload_json.dump();
            resultData["requestPayload"] = QString::fromStdString(request_body_str);

            httplib::Client cli(host.c_str(), 443);
            cli.enable_server_certificate_verification(true);
            cli.set_connection_timeout(0, 300000); 
            cli.set_read_timeout(60, 0); // Increased read timeout to 60s

            std::string path = "/v1beta/models/" + currentModelId.toStdString() + ":generateContent?key=" + currentApiKey.toStdString();
            
            httplib::Headers headers = { {"Content-Type", "application/json"} };
            httplib::Result http_res = cli.Post(path.c_str(), headers, request_body_str, "application/json");

            if (!http_res) {
                resultData["errorString"] = "HTTP client error: " + httplib::to_string(http_res.error());
                return resultData;
            }
            
            resultData["fullResponsePayload"] = QString::fromStdString(http_res->body);

            if (http_res->status != 200) {
                resultData["errorString"] = "API Error: HTTP Status " + QString::number(http_res->status) + ". Response: " + QString::fromStdString(http_res->body);
                return resultData;
            }

            nlohmann::json response_json = nlohmann::json::parse(http_res->body);
            
            if (response_json.contains("candidates") && response_json["candidates"].is_array() && !response_json["candidates"].empty()) {
                const auto& first_candidate = response_json["candidates"][0];
                if (first_candidate.contains("content") && first_candidate["content"].contains("parts") &&
                    first_candidate["content"]["parts"].is_array() && !first_candidate["content"]["parts"].empty()) {
                    const auto& first_part = first_candidate["content"]["parts"][0];
                    if (first_part.contains("text")) {
                        resultData["responseText"] = QString::fromStdString(first_part["text"].get<std::string>());
                    } else { resultData["errorString"] = "API Response: 'text' field missing."; }
                } else { resultData["errorString"] = "API Response: 'content' or 'parts' missing."; }
            } else if (response_json.contains("promptFeedback")) {
                std::string feedback_str = response_json["promptFeedback"].dump(2);
                resultData["errorString"] = "API Response: Prompt feedback received, potential block. Details: " + QString::fromStdString(feedback_str);
            } else { resultData["errorString"] = "API Response: 'candidates' field missing or empty."; }

        } catch (const nlohmann::json::parse_error& e) {
            resultData["errorString"] = "JSON Parse Error (Response): " + QString::fromUtf8(e.what());
        } catch (const nlohmann::json::exception& e) { 
            resultData["errorString"] = "JSON Data Error (Response): " + QString::fromUtf8(e.what());
        } catch (const std::exception& e) {
            resultData["errorString"] = "Standard Exception: " + QString::fromUtf8(e.what());
        } catch (...) {
            resultData["errorString"] = "Unknown error in API call thread.";
        }
        return resultData;
    });

    m_apiWatcher.setFuture(future);
}

void GeminiService::handleApiCallFinished() {
    QVariantMap result = m_apiWatcher.result();
    emit responseReady(
        result.value("responseText").toString(),
        result.value("requestPayload").toString(),
        result.value("fullResponsePayload").toString(),
        result.value("errorString").toString()
    );
}
