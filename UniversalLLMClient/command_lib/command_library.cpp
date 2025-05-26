#include "command_library.h"
#include <fstream>      
#include <iostream>     
#include <sstream>      
#include <cstdio>       
#include <array>        
#include <algorithm>    
#include <filesystem>   
#include <stdexcept> // For std::runtime_error in fromJson

#ifdef _WIN32
#include <direct.h> 
#else
#include <sys/stat.h> 
#endif

namespace CommandLib {

// --- CommandInfo Method Implementations ---

nlohmann::json CommandInfo::toJson() const {
    nlohmann::json j;
    j["name"] = name;
    j["description"] = description;
    j["isEnabled"] = isEnabled;
    j["requiresAdmin"] = requiresAdmin;

    // Manual enum to string conversion
    switch (type) {
        case CommandImplementationType::NATIVE_CPP:
            j["type"] = "NATIVE_CPP";
            break;
        case CommandImplementationType::OS_COMMAND:
            j["type"] = "OS_COMMAND";
            j["osCommandTemplate"] = osCommandTemplate;
            break;
        default:
            j["type"] = "UNKNOWN"; // Should not happen
            break;
    }
    // nativeFunction is not serialized
    return j;
}

CommandInfo CommandInfo::fromJson(const nlohmann::json& j) {
    CommandInfo ci;
    // Mandatory fields
    if (j.contains("name") && j.at("name").is_string()) {
        j.at("name").get_to(ci.name);
    } else {
        throw std::runtime_error("CommandInfo::fromJson: 'name' field is missing or not a string.");
    }

    if (j.contains("description") && j.at("description").is_string()) {
        j.at("description").get_to(ci.description);
    } else {
        ci.description = ""; // Default if missing
    }

    if (j.contains("isEnabled") && j.at("isEnabled").is_boolean()) {
        j.at("isEnabled").get_to(ci.isEnabled);
    } else {
        ci.isEnabled = true; // Default if missing
    }
    
    if (j.contains("requiresAdmin") && j.at("requiresAdmin").is_boolean()) {
        j.at("requiresAdmin").get_to(ci.requiresAdmin);
    } else {
        ci.requiresAdmin = false; // Default if missing
    }

    // Manual string to enum conversion
    if (j.contains("type") && j.at("type").is_string()) {
        std::string typeStr;
        j.at("type").get_to(typeStr);
        if (typeStr == "NATIVE_CPP") {
            ci.type = CommandImplementationType::NATIVE_CPP;
        } else if (typeStr == "OS_COMMAND") {
            ci.type = CommandImplementationType::OS_COMMAND;
            if (j.contains("osCommandTemplate") && j.at("osCommandTemplate").is_string()) {
                j.at("osCommandTemplate").get_to(ci.osCommandTemplate);
            } else {
                // OS_COMMAND type should ideally have a template.
                // If it's missing, this command might be invalid.
                // For robustness, allow it but it might fail on execution if template is needed.
                ci.osCommandTemplate = ""; 
                std::cerr << "Warning: CommandInfo::fromJson: OS_COMMAND '" << ci.name << "' is missing 'osCommandTemplate'." << std::endl;
            }
        } else {
            throw std::runtime_error("CommandInfo::fromJson: Unknown 'type' string: " + typeStr);
        }
    } else {
        throw std::runtime_error("CommandInfo::fromJson: 'type' field is missing or not a string for command: " + ci.name);
    }
    
    // nativeFunction is not deserialized, must be set programmatically for NATIVE_CPP commands
    return ci;
}


// --- Helper Functions (parseArguments, substituteArguments - unchanged from previous correct version) ---
std::vector<std::string> CommandManager::parseArguments(const std::string& argsString) const {
    std::vector<std::string> args;
    std::string currentArg;
    std::istringstream iss(argsString);
    char c;
    bool inQuotes = false;
    while (iss.get(c)) {
        if (c == '"') {
            inQuotes = !inQuotes;
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
        std::string escapedArg = args[i];
        if (escapedArg.find(' ') != std::string::npos || escapedArg.find('"') != std::string::npos) {
            size_t qpos = escapedArg.find('"');
            while(qpos != std::string::npos) {
                escapedArg.replace(qpos, 1, "\\\""); // Corrected escaping
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

// --- CommandManager Implementation (constructor, initializeDefaultCommands, etc. - largely unchanged but uses new CommandInfo methods) ---
CommandManager::CommandManager(std::string configFilePath) : m_configFilePath(std::move(configFilePath)) {
    initializeDefaultCommands(); 
    if (!loadConfiguration()) { 
        std::cout << "CommandLib: No valid configuration file found at " << m_configFilePath 
                  << ". Using/creating default commands. Consider saving configuration." << std::endl;
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
        }, false);

    registerNativeCommand("mkdir_native", "Creates a directory. Usage: mkdir_native <dirname>",
        [](const std::vector<std::string>& args) -> std::pair<std::string, std::string> {
            if (args.empty()) { return {"", "Error: mkdir_native requires a directory name."}; }
            try {
                if (std::filesystem::exists(args[0])) {
                     if (std::filesystem::is_directory(args[0])) { return {"Directory '" + args[0] + "' already exists.", ""}; } 
                     else { return {"", "Error: Path '" + args[0] + "' exists but is not a directory."}; }
                }
                if (std::filesystem::create_directory(args[0])) { return {"Directory '" + args[0] + "' created successfully.", ""}; } 
                else { return {"", "Error: Could not create directory '" + args[0] + "' (unknown reason)."}; }
            } catch (const std::filesystem::filesystem_error& e) { return {"", "Filesystem error: " + std::string(e.what())}; }
        }, false); 

    addConfigurableCommand(CommandInfo("python", "Executes a python script or command.", "python %ARGS_ALL%", true, false));
    addConfigurableCommand(CommandInfo("git", "Executes a git command.", "git %ARGS_ALL%", true, false));
    #ifdef _WIN32
    addConfigurableCommand(CommandInfo("ls_os", "Lists directory contents (Windows DIR).", "dir %ARG[0]%", true, false));
    #else
    addConfigurableCommand(CommandInfo("ls_os", "Lists directory contents (Unix ls -la).", "ls -la %ARG[0]%", true, false));
    #endif
}

bool CommandManager::registerNativeCommand(const std::string& name, const std::string& description,
                                           CommandInfo::NativeCppFunction func, bool requiresAdmin) {
    if (m_commands.count(name)) { return false; }
    m_commands[name] = CommandInfo(name, description, func, true, requiresAdmin);
    return true;
}

bool CommandManager::addConfigurableCommand(const CommandInfo& cmdInfo) {
    auto it = m_commands.find(cmdInfo.name);
    if (it != m_commands.end() && it->second.type == CommandImplementationType::NATIVE_CPP) {
        it->second.description = cmdInfo.description;
        it->second.isEnabled = cmdInfo.isEnabled;
        it->second.requiresAdmin = cmdInfo.requiresAdmin;
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
    // Correctly extract the remainder of the string as arguments
    if (iss.tellg() != -1 && static_cast<size_t>(iss.tellg()) < fullCommandString.length()) { 
         size_t firstCharPos = fullCommandString.find_first_not_of(" \t\n\r\f\v", commandName.length());
         if (firstCharPos != std::string::npos) { argsString = fullCommandString.substr(firstCharPos); }
    }
    auto it = m_commands.find(commandName);
    if (it == m_commands.end()) { return {"", "Error: Command '" + commandName + "' not found."}; }
    const CommandInfo& cmdInfo = it->second;
    if (!cmdInfo.isEnabled) { return {"", "Error: Command '" + commandName + "' is disabled."}; }
    if (!checkPermissions(cmdInfo)) { return {"", "Error: Insufficient permissions."}; }
    std::vector<std::string> args = parseArguments(argsString);
    if (cmdInfo.type == CommandImplementationType::NATIVE_CPP) {
        if (cmdInfo.nativeFunction) {
            try { return cmdInfo.nativeFunction(args); } 
            catch (const std::exception& e) { return {"", "Native command error: " + std::string(e.what())}; }
        } else { return {"", "Error: Native command '" + commandName + "' no implementation."}; }
    } else if (cmdInfo.type == CommandImplementationType::OS_COMMAND) {
        if (cmdInfo.osCommandTemplate.empty()) { return {"", "Error: OS command '" + commandName + "' no template."}; }
        std::string finalOsCommand = substituteArguments(cmdInfo.osCommandTemplate, args);
        return captureOSCommandOutput(finalOsCommand);
    }
    return {"", "Error: Unknown command type."};
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
        } else { helpText << "Command '" << commandName << "' not found." << std::endl; }
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
    for (const auto& pair : m_commands) { list.push_back(pair.second); }
    return list;
}
const CommandInfo* CommandManager::getCommandInfo(const std::string& name) const {
    auto it = m_commands.find(name);
    return (it != m_commands.end()) ? &(it->second) : nullptr;
}
bool CommandManager::enableCommand(const std::string& name, bool enable) {
    auto it = m_commands.find(name);
    if (it != m_commands.end()) { it->second.isEnabled = enable; return true; }
    return false;
}
bool CommandManager::isCommandEnabled(const std::string& name) const {
    auto it = m_commands.find(name);
    return (it != m_commands.end()) ? it->second.isEnabled : false;
}

// Updated loadConfiguration and saveConfiguration
bool CommandManager::loadConfiguration() {
    std::ifstream configFile(m_configFilePath);
    if (!configFile.is_open()) { return false; }
    try {
        nlohmann::json j_array; // Changed variable name to avoid conflict
        configFile >> j_array; 
        if (j_array.is_discarded() || !j_array.is_array()) {
            std::cerr << "CommandLib: Config " << m_configFilePath << " not a valid JSON array." << std::endl;
            return false;
        }
        
        for (const auto& item_json : j_array) { // Changed variable name
            try {
                CommandInfo loadedCmdInfo = CommandInfo::fromJson(item_json); // Use new static method
                
                auto existingCmdIt = m_commands.find(loadedCmdInfo.name);
                if (existingCmdIt != m_commands.end()) {
                    existingCmdIt->second.description = loadedCmdInfo.description;
                    existingCmdIt->second.isEnabled = loadedCmdInfo.isEnabled;
                    existingCmdIt->second.requiresAdmin = loadedCmdInfo.requiresAdmin;
                    if (existingCmdIt->second.type == CommandImplementationType::OS_COMMAND &&
                        loadedCmdInfo.type == CommandImplementationType::OS_COMMAND) {
                        existingCmdIt->second.osCommandTemplate = loadedCmdInfo.osCommandTemplate;
                    }
                } else {
                    if (loadedCmdInfo.type == CommandImplementationType::OS_COMMAND) {
                         m_commands[loadedCmdInfo.name] = loadedCmdInfo;
                    }
                }
            } catch (const std::exception& e) { 
                std::cerr << "CommandLib: Error parsing a command entry in " << m_configFilePath << ": " << e.what() << std::endl;
                // Continue to try loading other entries
            }
        }
    } catch (const nlohmann::json::exception& e) { // Ensure nlohmann namespace is correct
        std::cerr << "CommandLib: Error parsing JSON structure from " << m_configFilePath << ": " << e.what() << std::endl;
        return false;
    } catch (const std::exception& e) {
        std::cerr << "CommandLib: Generic error reading " << m_configFilePath << ": " << e.what() << std::endl;
        return false;
    }
    return true;
}

bool CommandManager::saveConfiguration() {
    nlohmann::json j_array = nlohmann::json::array(); 
    for (const auto& pair : m_commands) {
         j_array.push_back(pair.second.toJson()); // Use new instance method
    }
    std::ofstream configFile(m_configFilePath);
    if (!configFile.is_open()) {
        std::cerr << "CommandLib: Error opening config for writing: " << m_configFilePath << std::endl;
        return false;
    }
    try {
        configFile << j_array.dump(4); 
    } catch (const nlohmann::json::exception& e) { // Ensure nlohmann namespace is correct
         std::cerr << "CommandLib: Error serializing commands to JSON: " << e.what() << std::endl;
        return false;
    }
    return true;
}

bool CommandManager::checkPermissions(const CommandInfo& cmd, const std::string& ) const { 
    if (cmd.requiresAdmin) { return true; } return true;
}
std::pair<std::string, std::string> CommandManager::captureOSCommandOutput(const std::string& command) { 
    std::string stdout_str, stderr_str; 
    std::array<char, 256> buffer; 
    if (command.empty()) { return {"", "Error: Empty command."}; }
    std::string cmd_with_stderr = command + " 2>&1";
    FILE* pipe = nullptr;
    #ifdef _WIN32
    pipe = _popen(cmd_with_stderr.c_str(), "r");
    #else
    pipe = popen(cmd_with_stderr.c_str(), "r");
    #endif
    if (!pipe) { stderr_str = "Error: popen() failed for: " + command; return {stdout_str, stderr_str}; }
    try {
        while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) { stdout_str += buffer.data(); }
    } catch (...) { stderr_str = "Error reading pipe."; }
    int rc = 0;
    #ifdef _WIN32
    rc = _pclose(pipe);
    #else
    rc = pclose(pipe);
    if (WIFEXITED(rc)) { rc = WEXITSTATUS(rc); } else { rc = -1; if (stderr_str.empty()) stderr_str = "Cmd terminated abnormally."; }
    #endif
    if (rc != 0 && stderr_str.empty()) { stderr_str = "Cmd exited with code " + std::to_string(rc) + "."; }
    return {stdout_str, stderr_str};
}

} // namespace CommandLib
