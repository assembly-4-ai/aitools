cmake_minimum_required(VERSION 3.16)
project(UniversalLLMClient VERSION 1.0.0 LANGUAGES CXX) # Added VERSION and LANGUAGES

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_EXTENSIONS OFF) # Good practice to disable compiler-specific extensions

# --- Qt Configuration ---
find_package(Qt6 REQUIRED COMPONENTS Widgets Core Concurrent Network) # Added Network for QUrl etc.
set(QT_VERSION 6) # Used later for linking Qt${QT_VERSION}::Widgets

# Enable automatic MOC (Meta-Object Compiler), UIC (User Interface Compiler),
# and RCC (Resource Compiler) processing.
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON) # If you add a .qrc resource file
set(CMAKE_AUTOUIC ON) # If you use .ui files from Qt Designer

# --- Find OpenSSL ---
# Ensure OPENSSL_ROOT_DIR is set if OpenSSL is not in a standard system path
# e.g., cmake -DOPENSSL_ROOT_DIR=/path/to/openssl ..
find_package(OpenSSL REQUIRED)

# --- Include Directories ---
# Order can sometimes matter, though usually not critically for these.
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}                   # Project root (for top-level includes if any)
    ${CMAKE_CURRENT_SOURCE_DIR}/ui                # For mainwindow.h/cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/llm_services      # For service classes
    ${CMAKE_CURRENT_SOURCE_DIR}/logging           # For session_logger
    ${CMAKE_CURRENT_SOURCE_DIR}/command_lib       # For command_library
    # ${CMAKE_CURRENT_SOURCE_DIR}/orchestration   # Uncomment if you have this
    # ${CMAKE_CURRENT_SOURCE_DIR}/terminal_interaction # Uncomment if you have this
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor            # For httplib and nlohmann/json
    ${OpenSSL_INCLUDE_DIR}                        # For OpenSSL headers
    # Qt include directories are typically handled by target_link_libraries with Qt6::* targets
)

# --- Source Files ---
# Group source files for better organization (optional but good practice)
set(UI_SOURCES
    ui/main.cpp # Assuming main.cpp is also in ui/ with mainwindow
    ui/mainwindow.h
    ui/mainwindow.cpp
)

set(LLM_SERVICE_SOURCES
    llm_services/llm_service.h
    llm_services/gemini_service.h
    llm_services/gemini_service.cpp
    llm_services/ollama_service.h
    llm_services/ollama_service.cpp
    llm_services/openai_compatible_service.h
    llm_services/openai_compatible_service.cpp
)

set(LOGGING_SOURCES
    logging/session_logger.h
    logging/session_logger.cpp
)

set(COMMAND_LIB_SOURCES
    command_lib/command_library.h
    command_lib/command_library.cpp
)

# set(ORCHESTRATION_SOURCES # Uncomment and populate if you have these
# orchestration/orchestrator_manager.h
# orchestration/orchestrator_manager.cpp
# )

# set(TERMINAL_SOURCES # Uncomment and populate if you have these
# terminal_interaction/terminal_process_handler.h
# terminal_interaction/terminal_process_handler.cpp
# )

# Executable Target
add_executable(${PROJECT_NAME}
    ${UI_SOURCES}
    ${LLM_SERVICE_SOURCES}
    ${LOGGING_SOURCES}
    ${COMMAND_LIB_SOURCES}
    # ${ORCHESTRATION_SOURCES}
    # ${TERMINAL_SOURCES}
)

# --- Preprocessor Definitions ---
target_compile_definitions(${PROJECT_NAME} PRIVATE
    QT_DEPRECATED_WARNINGS # Enable warnings for deprecated Qt features
    # Define CPPHTTPLIB_OPENSSL_SUPPORT if httplib needs to use OpenSSL (for HTTPS)
    # This should be defined in your .cpp files before including httplib.h,
    # or globally here if all httplib usage needs it.
    # Example: CPPHTTPLIB_OPENSSL_SUPPORT
)
# If you put #define CPPHTTPLIB_OPENSSL_SUPPORT in your .cpp files before #include "httplib.h",
# then you don't strictly need it here, but it doesn't hurt.

# --- Link Libraries ---
target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt${QT_VERSION}::Widgets
    Qt${QT_VERSION}::Core
    Qt${QT_VERSION}::Concurrent
    Qt${QT_VERSION}::Network # For QUrl, QNetworkAccessManager (if you switch httplib out)

    ${OpenSSL_LIBRARIES}      # Link against OpenSSL (e.g., ssl crypto)
)

# For some systems using cpp-httplib with SSL, pthreads might be needed,
# especially on Linux. For Windows with MSVC, this is usually not an issue.
if (UNIX AND NOT APPLE AND NOT MSVC) # More specific condition
    find_package(Threads REQUIRED)
    target_link_libraries(${PROJECT_NAME} PRIVATE Threads::Threads)
endif()

# --- C++ Standard Features ---
# This ensures specific C++17 features are available if your compiler needs flags.
# Usually covered by CMAKE_CXX_STANDARD, but can be more explicit.
# target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_17)


# --- Platform Specific Settings ---
if(MSVC)
    # Add specific MSVC flags if needed, e.g., for warnings
    # target_compile_options(${PROJECT_NAME} PRIVATE /W4)
endif()

if(APPLE)
    # macOS specific settings, e.g., creating an application bundle
    set_target_properties(${PROJECT_NAME} PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist" # Create this file
    )
    # You might need to find and link frameworks like CoreFoundation, etc.
    # find_library(COREFOUNDATION_LIBRARY CoreFoundation)
    # target_link_libraries(${PROJECT_NAME} PRIVATE ${COREFOUNDATION_LIBRARY})
endif()

# --- Installation (Optional) ---
# Defines how to install the application and its dependencies.
# This is more for creating distributable packages.
# Example:
# install(TARGETS ${PROJECT_NAME}
# RUNTIME DESTINATION bin
# LIBRARY DESTINATION lib
# ARCHIVE DESTINATION lib
# )
# install(FILES vendor/httplib.h DESTINATION include) # Example if distributing headers

# --- For easier development with IDEs like VS Code or CLion ---
set(CMAKE_EXPORT_COMPILE_COMMANDS ON) # Generates compile_commands.json

# --- Output ---
message(STATUS "Project Name: ${PROJECT_NAME}")
message(STATUS "CMAKE_CXX_COMPILER: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "Qt Version: ${Qt6_VERSION}")
message(STATUS "OpenSSL Version: ${OpenSSL_VERSION}")
message(STATUS "OpenSSL Include Dir: ${OpenSSL_INCLUDE_DIR}")
message(STATUS "OpenSSL Libraries: ${OpenSSL_LIBRARIES}")

# --- End of CMakeLists.txt ---