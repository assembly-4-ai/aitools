cmake_minimum_required(VERSION 3.16)
project(UniversalLLMClient VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Add NOMINMAX definition for MSVC builds to avoid conflicts with std::min/max
if(MSVC)
    add_definitions(-DNOMINMAX)
endif()

# Find Qt6
# Note: Qt6::Concurrent is added here as it's needed by callGeminiApi
find_package(Qt6 COMPONENTS Widgets Core Concurrent Network REQUIRED)

# Find OpenSSL
find_package(OpenSSL REQUIRED)

# Include directories
include_directories(
    "${CMAKE_CURRENT_SOURCE_DIR}" 
    "${CMAKE_CURRENT_SOURCE_DIR}/vendor"
    "${CMAKE_CURRENT_SOURCE_DIR}/common"
    "${CMAKE_CURRENT_SOURCE_DIR}/ui"
    "${CMAKE_CURRENT_SOURCE_DIR}/llm_services" 
    "${CMAKE_CURRENT_SOURCE_DIR}/command_lib" 
    "${CMAKE_CURRENT_SOURCE_DIR}/logging" 
    "${CMAKE_CURRENT_SOURCE_DIR}/orchestration" 
    "${CMAKE_CURRENT_SOURCE_DIR}/terminal_interaction" 
)

# Set search path for .ui files if AutoUic is used (good practice)
set(CMAKE_AUTOUIC_SEARCH_PATHS "${CMAKE_CURRENT_SOURCE_DIR}/ui")

# Define the executable and add sources
add_executable(UniversalLLMClient
    ui/main.cpp
    ui/mainwindow.cpp
    llm_services/llm_service.cpp
    llm_services/gemini_service.cpp 
    llm_services/ollama_service.cpp
    llm_services/openai_compatible_service.cpp
    command_lib/command_library.cpp
    logging/session_logger.cpp
    orchestration/orchestrator_manager.cpp
    terminal_interaction/terminal_process_handler.cpp
)

# Link Qt and OpenSSL libraries
target_link_libraries(UniversalLLMClient PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::Network
    Qt6::Concurrent # For QtConcurrent::run used in mainwindow.cpp
    OpenSSL::SSL
    OpenSSL::Crypto
)
