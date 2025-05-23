#include "llm_services/llm_service.h" // Adjusted

LLMService::LLMService(QObject* parent) : QObject(parent) {}

QString LLMService::getLastSelectedModel() const {
    return m_lastSelectedModel;
}

void LLMService::setLastSelectedModel(const QString& modelName) {
    m_lastSelectedModel = modelName;
}