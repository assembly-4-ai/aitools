#include "command_library.h"
#include <fstream>      
#include <iostream>     
#include <sstream>      
#include <cstdio>       
#include <array>        
#include <algorithm>    
#include <filesystem>   // For native mkdir, and path operations (C++17)

#ifdef _WIN32
#include <direct.h> 
// No need for MKDIR macro if using std::filesystem::create_directory
#else
#include <sys/stat.h> 
// No need for MKDIR macro if using std::filesystem::create_directory
#endif

namespace CommandLib { // Wrap entire implementation in namespace

// --- Helper Functions ---

std::vector<std::string> CommandManager::parseArguments(const std::string& argsString) const {
    std::vector<std::string> args;
    std::string currentArg;
    std::istringstream iss(argsString);
    char c;
    bool inQuotes = false;

    while (iss.get(c)) {
        if (c == '"') {
            inQuotes = !inQuotes;
            // If ending a quoted argument and it's not empty, or if starting a new one with content
            if (!currentArg.empty() && !inQuotes) { 
                // args.push_back(currentArg); // Don't push yet, wait for space or end
                // currentArg.clear();
            } else if (inQuotes && !currentArg.empty()){
                 currentArg += c; 
            } // else: quote is first char of arg, or empty arg being quoted.
        } else if (std::isspace(c) && !inQuotes) {
            if (!currentArg.empty()) {
                args.push_back(currentArg);
                currentArg.clear();
            }
        } else {
            currentArg += c;
        }
    }
    if (!currentArg.empty()) {
        args.push_back(currentArg);
    }
    return args;
}

std::string CommandManager::substituteArguments(std::string templateStr, const std::vector<std::string>& args) const {
    std::string result = templateStr;
    std::string allArgsJoined;
    for (size_t i = 0; i < args.size(); ++i) {
        if (i > 0) allArgsJoined += " ";
        // Basic escaping: wrap in quotes. Real shell escaping is much harder.
        std::string escapedArg = args[i];
        // Minimal check: if arg contains spaces or quotes, wrap it.
        if (escapedArg.find(' ') != std::string::npos || escapedArg.find('"') != std::string::npos) {
            // Replace internal quotes if any (very basic)
            size_t qpos = escapedArg.find('"');
            while(qpos != std::string::npos) {
                escapedArg.replace(qpos, 1, "\\\""); 
                qpos = escapedArg.find('"', qpos + 2);
            }
            escapedArg = "\"" + escapedArg + "\"";
        }
        allArgsJoined += escapedArg; 
        
        std::string argPlaceholder = "%ARG[" + std::to_string(i) + "]%";
        size_t pos = result.find(argPlaceholder);
        while (pos != std::string::npos) {
            result.replace(pos, argPlaceholder.length(), escapedArg);
            pos = result.find(argPlaceholder, pos + escapedArg.length());
        }
    }

    size_t pos_all = result.find("%ARGS_ALL%");
    while (pos_all != std::string::npos) {
        result.replace(pos_all, std::string("%ARGS_ALL%").length(), allArgsJoined);
        pos_all = result.find("%ARGS_ALL%", pos_all + allArgsJoined.length());
    }
    return result;
}

// --- CommandManager Implementation ---

CommandManager::CommandManager(std::string configFilePath) : m_configFilePath(std::move(configFilePath)) {
    initializeDefaultCommands(); 
    if (!loadConfiguration()) { 
        std::cout << "CommandLib: No valid configuration file found at " << m_configFilePath 
                  << ". Using/creating default commands. Consider saving configuration." << std::endl;
        // saveConfiguration(); // Optionally create and save a new config with defaults
    }
}

void CommandManager::initializeDefaultCommands() {
    registerNativeCommand("echo", "Echos back the provided arguments.", 
        [](const std::vector<std::string>& args) -> std::pair<std::string, std::string> {
            std::string echo_out;
            for (size_t i = 0; i < args.size(); ++i) {
                echo_out += args[i] + (i == args.size() - 1 ? "" : " ");
            }
            return {echo_out, ""}; 
        }, false); // requiresAdmin = false

    registerNativeCommand("mkdir_native", "Creates a directory. Usage: mkdir_native <dirname>",
        [](const std::vector<std::string>& args) -> std::pair<std::string, std::string> {
            if (args.empty()) {
                return {"", "Error: mkdir_native requires a directory name."};
            }
            try {
                // Check if path is relative and make it absolute for clarity if needed, or ensure current directory is well-defined.
                // For simplicity, using path as is.
                if (std::filesystem::exists(args[0])) {
                     if (std::filesystem::is_directory(args[0])) {
                         return {"Directory '" + args[0] + "' already exists.", ""};
                     } else {
                         return {"", "Error: Path '" + args[0] + "' exists but is not a directory."};
                     }
                }
                if (std::filesystem::create_directory(args[0])) {
                    return {"Directory '" + args[0] + "' created successfully.", ""};
                } else {
                    return {"", "Error: Could not create directory '" + args[0] + "' (unknown reason)."};
                }
            } catch (const std::filesystem::filesystem_error& e) {
                return {"", "Filesystem error creating directory '" + args[0] + "': " + std::string(e.what())};
            }
        }, false); 

    // OS Wrapper Commands
    // Constructors for CommandInfo were updated to include requiresAdmin
    addConfigurableCommand(CommandInfo("python", "Executes a python script or command. Args are passed to python.", "python %ARGS_ALL%", true, false));
    addConfigurableCommand(CommandInfo("git", "Executes a git command. Args are passed to git.", "git %ARGS_ALL%", true, false));
    
    #ifdef _WIN32
    addConfigurableCommand(CommandInfo("ls_os", "Lists directory contents (Windows DIR). Example: ls_os C:\\path", "dir %ARG[0]%", true, false));
    #else
    addConfigurableCommand(CommandInfo("ls_os", "Lists directory contents (Unix ls -la). Example: ls_os /path", "ls -la %ARG[0]%", true, false));
    #endif
}

bool CommandManager::registerNativeCommand(const std::string& name, const std::string& description,
                                           CommandInfo::NativeCppFunction func, bool requiresAdmin) {
    if (m_commands.count(name)) {
        return false; 
    }
    // Use the correct CommandInfo constructor for native functions
    m_commands[name] = CommandInfo(name, description, func, true, requiresAdmin);
    return true;
}

bool CommandManager::addConfigurableCommand(const CommandInfo& cmdInfo) {
    // If command exists, this will update it. If it's a native command, its function pointer won't be overwritten by this method
    // as CommandInfo from config won't have a nativeFunction set by JSON deserialization.
    // However, other properties like isEnabled, description could be updated.
    auto it = m_commands.find(cmdInfo.name);
    if (it != m_commands.end() && it->second.type == CommandImplementationType::NATIVE_CPP) {
        // Preserve native function, update other fields
        it->second.description = cmdInfo.description;
        it->second.isEnabled = cmdInfo.isEnabled;
        it->second.requiresAdmin = cmdInfo.requiresAdmin;
        // Do not change type or osCommandTemplate for an existing native command via this route
    } else {
        m_commands[cmdInfo.name] = cmdInfo;
    }
    return true;
}

bool CommandManager::unregisterCommand(const std::string& name) {
    return m_commands.erase(name) > 0;
}

std::pair<std::string, std::string> CommandManager::executeCommand(const std::string& fullCommandString) {
    std::istringstream iss(fullCommandString);
    std::string commandName;
    iss >> commandName; 

    std::string argsString;
    if (iss.tellg() != -1 && static_cast<size_t>(iss.tellg()) < fullCommandString.length()) { // Check if there's anything left after command name
         size_t firstCharPos = fullCommandString.find_first_not_of(" \t\n\r\f\v", commandName.length());
         if (firstCharPos != std::string::npos) {
            argsString = fullCommandString.substr(firstCharPos);
         }
    }


    auto it = m_commands.find(commandName);
    if (it == m_commands.end()) {
        return {"", "Error: Command '" + commandName + "' not found."};
    }

    const CommandInfo& cmdInfo = it->second;
    if (!cmdInfo.isEnabled) {
        return {"", "Error: Command '" + commandName + "' is disabled."};
    }

    if (!checkPermissions(cmdInfo)) { // Placeholder check
         return {"", "Error: Insufficient permissions to execute command '" + commandName + "'."};
    }

    std::vector<std::string> args = parseArguments(argsString);

    if (cmdInfo.type == CommandImplementationType::NATIVE_CPP) {
        if (cmdInfo.nativeFunction) {
            try {
                return cmdInfo.nativeFunction(args);
            } catch (const std::exception& e) {
                return {"", "Error executing native command '" + commandName + "': " + e.what()};
            }
        } else {
            return {"", "Error: Native command '" + commandName + "' has no implementation registered."};
        }
    } else if (cmdInfo.type == CommandImplementationType::OS_COMMAND) {
        if (cmdInfo.osCommandTemplate.empty()) {
            return {"", "Error: OS command '" + commandName + "' has no command template."};
        }
        std::string finalOsCommand = substituteArguments(cmdInfo.osCommandTemplate, args);
        return captureOSCommandOutput(finalOsCommand);
    }

    return {"", "Error: Unknown command type for '" + commandName + "'."};
}

std::string CommandManager::getHelp(const std::string& commandName) const {
    std::ostringstream helpText;
    if (!commandName.empty()) {
        auto it = m_commands.find(commandName);
        if (it != m_commands.end()) {
            const auto& cmd = it->second;
            helpText << cmd.name << ": " << cmd.description 
                     << (cmd.isEnabled ? " (Enabled)" : " (Disabled)") << std::endl;
            if (cmd.type == CommandImplementationType::OS_COMMAND) {
                helpText << "  Template: " << cmd.osCommandTemplate << std::endl;
            }
        } else {
            helpText << "Command '" << commandName << "' not found." << std::endl;
        }
    } else {
        helpText << "Available commands (" << m_commands.size() << " total):\n";
        for (const auto& pair : m_commands) {
            const auto& cmd = pair.second;
            helpText << "  " << cmd.name << (cmd.isEnabled ? "" : " (Disabled)") << ": " << cmd.description << std::endl;
        }
    }
    return helpText.str();
}

std::vector<CommandInfo> CommandManager::getCommandList() const {
    std::vector<CommandInfo> list;
    list.reserve(m_commands.size());
    for (const auto& pair : m_commands) {
        list.push_back(pair.second);
    }
    return list;
}

const CommandInfo* CommandManager::getCommandInfo(const std::string& name) const {
    auto it = m_commands.find(name);
    return (it != m_commands.end()) ? &(it->second) : nullptr;
}

bool CommandManager::enableCommand(const std::string& name, bool enable) {
    auto it = m_commands.find(name);
    if (it != m_commands.end()) {
        it->second.isEnabled = enable;
        return true;
    }
    return false;
}

bool CommandManager::isCommandEnabled(const std::string& name) const {
    auto it = m_commands.find(name);
    return (it != m_commands.end()) ? it->second.isEnabled : false;
}

bool CommandManager::loadConfiguration() {
    std::ifstream configFile(m_configFilePath);
    if (!configFile.is_open()) {
        return false; 
    }
    try {
        nlohmann::json j;
        configFile >> j; // This can throw if file is malformed, but nlohmann often doesn't
        if (j.is_discarded() || !j.is_array()) { // Check if parsing failed or not an array
             std::cerr << "CommandLib: Configuration file " << m_configFilePath << " is not a valid JSON array." << std::endl;
            return false;
        }
        
        for (const auto& item : j) {
            CommandInfo loadedCmdInfo; // Default construct
            // Manually deserialize to avoid issues with std::function and ensure robustness
            if (item.contains("name") && item["name"].is_string()) item.at("name").get_to(loadedCmdInfo.name); else continue; // Name is mandatory
            if (item.contains("description") && item["description"].is_string()) item.at("description").get_to(loadedCmdInfo.description);
            if (item.contains("isEnabled") && item["isEnabled"].is_boolean()) item.at("isEnabled").get_to(loadedCmdInfo.isEnabled);
            if (item.contains("type") && item["type"].is_string()) { // Assuming enum is serialized as string by our adl_serializer
                 loadedCmdInfo.type = item.at("type").get<CommandImplementationType>();
            } else if (item.contains("type") && item["type"].is_number_integer()) { // Fallback for older enum int serialization
                 loadedCmdInfo.type = static_cast<CommandImplementationType>(item.at("type").get<int>());
            }


            if (item.contains("requiresAdmin") && item["requiresAdmin"].is_boolean()) item.at("requiresAdmin").get_to(loadedCmdInfo.requiresAdmin);

            if (loadedCmdInfo.type == CommandImplementationType::OS_COMMAND) {
                if (item.contains("osCommandTemplate") && item["osCommandTemplate"].is_string()) {
                    item.at("osCommandTemplate").get_to(loadedCmdInfo.osCommandTemplate);
                }
            }
            
            auto existingCmdIt = m_commands.find(loadedCmdInfo.name);
            if (existingCmdIt != m_commands.end()) {
                // Update existing command, but preserve native function
                existingCmdIt->second.description = loadedCmdInfo.description;
                existingCmdIt->second.isEnabled = loadedCmdInfo.isEnabled;
                existingCmdIt->second.requiresAdmin = loadedCmdInfo.requiresAdmin;
                if (existingCmdIt->second.type == CommandImplementationType::OS_COMMAND &&
                    loadedCmdInfo.type == CommandImplementationType::OS_COMMAND) {
                    existingCmdIt->second.osCommandTemplate = loadedCmdInfo.osCommandTemplate;
                }
                // Don't change type of an existing command if it's native.
            } else {
                // Only add from config if it's an OS command; native commands must be registered by code.
                if (loadedCmdInfo.type == CommandImplementationType::OS_COMMAND) {
                     m_commands[loadedCmdInfo.name] = loadedCmdInfo; // Use a copy
                }
            }
        }
    } catch (const nlohmann::json::exception& e) {
        std::cerr << "CommandLib: Error processing JSON from " << m_configFilePath << ": " << e.what() << std::endl;
        return false;
    } catch (const std::exception& e) {
        std::cerr << "CommandLib: Generic error reading " << m_configFilePath << ": " << e.what() << std::endl;
        return false;
    }
    return true;
}

bool CommandManager::saveConfiguration() {
    nlohmann::json j = nlohmann::json::array();
    for (const auto& pair : m_commands) {
        // Only serialize OS commands or native commands without their function pointer
        // The to_json for CommandInfo already handles not serializing nativeFunction
        j.push_back(pair.second); 
    }
    std::ofstream configFile(m_configFilePath);
    if (!configFile.is_open()) {
        std::cerr << "CommandLib: Error opening configuration file for writing: " << m_configFilePath << std::endl;
        return false;
    }
    try {
        configFile << j.dump(4); 
    } catch (const nlohmann::json::exception& e) {
         std::cerr << "CommandLib: Error serializing commands to JSON: " << e.what() << std::endl;
        return false;
    }
    return true;
}

bool CommandManager::checkPermissions(const CommandInfo& cmd, const std::string& /*userName*/) const {
    if (cmd.requiresAdmin) {
        // std::cout << "Command '" << cmd.name << "' requires admin privileges (not implemented, allowing for now)." << std::endl;
        return true; 
    }
    return true;
}

std::pair<std::string, std::string> CommandManager::captureOSCommandOutput(const std::string& command) {
    std::string stdout_str, stderr_str; // stderr_str may not be separately captured by simple popen
    std::array<char, 256> buffer; 

    // Ensure command is not empty
    if (command.empty()) {
        return {"", "Error: Empty command passed to captureOSCommandOutput."};
    }

    // Redirect stderr to stdout to capture both
    std::string command_with_stderr_redirect = command + " 2>&1";
    FILE* pipe = nullptr;
#ifdef _WIN32
    pipe = _popen(command_with_stderr_redirect.c_str(), "r");
#else
    pipe = popen(command_with_stderr_redirect.c_str(), "r");
#endif

    if (!pipe) {
        stderr_str = "Error: popen() failed for command: " + command;
        return {stdout_str, stderr_str};
    }

    try {
        while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
            stdout_str += buffer.data();
        }
    } catch (...) { // Should ideally be std::exception or specific file errors
        stderr_str = "Error reading pipe output.";
        // Fall through to pclose
    }

    int return_code = 0;
#ifdef _WIN32
    return_code = _pclose(pipe);
#else
    return_code = pclose(pipe);
    // On POSIX, pclose returns the shell's exit status. Need to check it.
    if (WIFEXITED(return_code)) {
        return_code = WEXITSTATUS(return_code); // Get the actual exit code if exited normally
    } else {
        // Process did not terminate normally (e.g., killed by a signal)
        return_code = -1; // Indicate abnormal termination, or use specific signal codes
        if (stderr_str.empty()) stderr_str = "Command terminated abnormally.";
    }
#endif

    if (return_code != 0) {
        if (stderr_str.empty()) { 
            // If stderr_str is still empty, it means the error message might be in stdout_str (due to 2>&1)
            // Or the command just returned a non-zero exit code without specific stderr.
            stderr_str = "Command exited with code " + std::to_string(return_code) + ".";
            // If stdout_str contains error-like messages, it's now effectively stderr.
        }
    }
    
    // If command produced output to stdout and also returned an error code,
    // stdout_str contains that output, and stderr_str contains the error message.
    return {stdout_str, stderr_str};
}

} // namespace CommandLib
