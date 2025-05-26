#include "llm_service.h"

// LLMService class implementation

// Constructor and destructor are defaulted in the header.
// Virtual methods like getName, isConfigured, configureService, fetchModels, generateResponse are pure virtual.
// getLastSelectedModel and setLastSelectedModel are defined inline in the header.

// This file is intentionally minimal for now. It serves as a place for:
// 1. Definitions of any future non-pure virtual methods that are not inline.
// 2. Any static members or helper functions private to the LLMService hierarchy.
// 3. Potentially moving inline getters/setters here if they grow more complex.

// For now, it just needs to exist to be added to the CMake build system
// and to confirm the linkage of the LLMService class if it had non-inline parts.
