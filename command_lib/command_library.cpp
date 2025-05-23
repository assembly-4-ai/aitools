#include "command_lib/command_library.h" // Adjusted
#include <sstream>
#include <fstream>
#include <cstdlib>
#include <algorithm>
#include <iostream> // For cerr, cout debugging
#include <regex>    // For placeholder replacement

#ifdef _WIN32
    #define OS_CMD_PREFIX "cmd /C "
#else
    #define OS_CMD_PREFIX ""
#endif

namespace fs = std::filesystem;

namespace CommandLibrary {

// Manual (de)serialization for CommandInfo
void to_json(json& j, const CommandInfo& ci) {
    j = json{
        {"description", ci.description},
        {"help_arguments", ci.help_arguments},
        {"isEnabled", ci.isEnabled},
        {"implementationType", ci.implementationType},
        {"osCommandTemplate", ci.osCommandTemplate}
    };
}

void from_json(const json& j, CommandInfo& ci) {
    j.at("description").get_to(ci.description);
    j.at("help_arguments").get_to(ci.help_arguments);
    j.at("isEnabled").get_to(ci.isEnabled);
    j.at("implementationType").get_to(ci.implementationType);
    // osCommandTemplate might not always be present
    if (j.contains("osCommandTemplate")) {
        j.at("osCommandTemplate").get_to(ci.osCommandTemplate);
    } else {
        ci.osCommandTemplate = "";
    }
}


std::string executeOSCommand_internal(const std::string& cmd_str, bool capture_output = true) {
    // Basic security check (very rudimentary)
    if (cmd_str.find("&&") != std::string::npos || cmd_str.find(";") != std::string::npos ||
        cmd_str.find("|") != std::string::npos || cmd_str.find("`") != std::string::npos) {
        // Allow '|' for piping if explicitly handled by a specific command wrapper
        // This generic check is to prevent simple concatenations.
        // A command template should be designed to avoid this for its specific args.
        // return "Error: Command contains potentially unsafe characters for direct OS execution.";
    }

    std::string full_cmd = OS_CMD_PREFIX + cmd_str;
    std::cout << "[DEBUG] Executing OS Command (internal): " << full_cmd << std::endl;

    if (!capture_output) {
        int result_code = system(full_cmd.c_str());
        if (result_code == 0) {
            return "Command executed (exit code 0). Output not captured.";
        } else {
            #ifdef _WIN32
            return "Error: Command execution failed (exit code " + std::to_string(result_code) + ").";
            #else
            return "Error: Command execution failed (exit code " + std::to_string(WEXITSTATUS(result_code)) + ").";
            #endif
        }
    }

    FILE* pipe = nullptr;
    std::string result_str_output;
    char buffer[256];

#ifdef _WIN32
    pipe = _popen(full_cmd.c_str(), "r");
#else
    pipe = popen(full_cmd.c_str(), "r");
#endif

    if (!pipe) {
        return "Error: Failed to execute command (popen failed).";
    }
    try {
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            result_str_output += buffer;
        }
    } catch (...) {
#ifdef _WIN32
        _pclose(pipe);
#else
        pclose(pipe);
#endif
        return "Error: Exception while reading command output.";
    }

    int exit_code_status = 0;
#ifdef _WIN32
    exit_code_status = _pclose(pipe);
#else
    exit_code_status = pclose(pipe);
#endif

    // On POSIX, pclose returns the shell's exit status.
    // On Windows, _pclose returns the process's exit status.
    int actual_exit_code = exit_code_status;
    #ifndef _WIN32
    if (WIFEXITED(exit_code_status)) {
        actual_exit_code = WEXITSTATUS(exit_code_status);
    } else {
        actual_exit_code = -1; // Indicate non-normal exit
    }
    #endif


    if (actual_exit_code != 0) {
         return "Command failed (exit code " + std::to_string(actual_exit_code) + "):\n" + result_str_output;
    }
    if (result_str_output.empty()){
        return "Command executed successfully (exit code " + std::to_string(actual_exit_code) + "). No output.";
    }
    return result_str_output;
}


std::string executeOSCommand(const std::string& cmd_template, const std::vector<std::string>& args) {
    // This function is meant for simple templates. The executeOSWrapperCommand in CommandManager
    // has more specific placeholder replacement logic.
    // This one is more of a raw passthrough, mainly used by C++ command functions.
    std::string final_cmd = cmd_template;
    for(const auto& arg : args) {
        // Simple space and quote handling. Not perfectly secure for all inputs.
        final_cmd += " ";
        if (arg.find(' ') != std::string::npos || arg.find('"') != std::string::npos || arg.find('\'') != std::string::npos) {
            std::string escaped_arg = arg;
            // Very basic escaping for quotes (Windows might need different for cmd.exe)
            size_t pos = 0;
            while ((pos = escaped_arg.find('"', pos)) != std::string::npos) {
                escaped_arg.replace(pos, 1, "\\\"");
                pos += 2;
            }
            final_cmd += "\"" + escaped_arg + "\"";
        } else {
            final_cmd += arg;
        }
    }
    return executeOSCommand_internal(final_cmd);
}

std::string executeRawOSCommand(const std::string& raw_cmd) {
    return executeOSCommand_internal(raw_cmd);
}


CommandManager::CommandManager(const std::string& configFilePath) : config_file_path_(configFilePath) {
    if (config_file_path_.empty()) { // Default if none provided
        config_file_path_ = "commands_config.json";
    }
    if (!loadConfiguration(config_file_path_)) {
        std::cerr << "Warning: Could not load command configuration from '" << config_file_path_
                  << "'. Initializing with default commands." << std::endl;
        initializeDefaultCommands();
        // saveConfiguration(config_file_path_); // Optionally save defaults immediately
    }
}

CommandManager::~CommandManager() {
    // saveConfiguration(config_file_path_); // Save on exit can be risky if app crashes.
}

void CommandManager::initializeDefaultCommands() {
    command_info_.clear();
    native_commands_.clear();

    // Example native command
    registerNativeCommand("echo",
        [](const std::vector<std::string>& args) -> std::string {
            std::string r;
            for (size_t i = 0; i < args.size(); ++i) {
                r += args[i];
                if (i < (args.size() - 1)) r += " ";
            }
            return r;
        },
        {"Prints the provided arguments.", {"-h", "--help"}, true, "native"});

    // Example OS Wrapper (can also be purely from config)
    CommandInfo mkdirInfo;
    mkdirInfo.description = "Creates a directory.";
    mkdirInfo.help_arguments = {"-h", "--help"};
    mkdirInfo.isEnabled = true;
    mkdirInfo.implementationType = "os_wrapper";
    #ifdef _WIN32
        mkdirInfo.osCommandTemplate = "mkdir \"%ARGS[0]%\"";
    #else
        mkdirInfo.osCommandTemplate = "mkdir -p \"%ARGS[0]%\"";
    #endif
    addConfigurableCommand("mkdir", mkdirInfo); // Or registerNativeCommand with a lambda calling executeOSCommand

    // Add many more defaults: g++, gcc, python, pip, apt, git, etc.
    // as either native (if they need C++ logic) or os_wrapper.
    // For brevity, I'm only showing a couple here. You need to add all from your list.

    CommandInfo pythonInfo;
    pythonInfo.description = "Runs a python script or interactive interpreter.";
    pythonInfo.help_arguments = {"-h", "--help", "/?"};
    pythonInfo.isEnabled = true;
    pythonInfo.implementationType = "os_wrapper";
    pythonInfo.osCommandTemplate = "python %ARGS_ALL%";
    addConfigurableCommand("python", pythonInfo);

    CommandInfo gccInfo;
    gccInfo.description = "GNU C Compiler.";
    gccInfo.help_arguments = {"-h", "--help", "/?"};
    gccInfo.isEnabled = true;
    gccInfo.implementationType = "os_wrapper";
    gccInfo.osCommandTemplate = "gcc %ARGS_ALL%";
    addConfigurableCommand("gcc", gccInfo);

    // ... and so on for all commands in your list ...
    // (nasm, clang, clang++, pip, apt, conda, npm, vim, vu, grep, wget, diff, git, hf, vcpkg , cmd, unzip, ld, qemu)
}

bool CommandManager::registerNativeCommand(const std::string& name, const CommandFunction& func, CommandInfo info) {
    if (command_info_.count(name)) {
        std::cerr << "Warning: Command '" << name << "' already exists. Overwriting." << std::endl;
    }
    info.implementationType = "native";
    command_info_[name] = info;
    native_commands_[name] = func;
    return true;
}

bool CommandManager::addConfigurableCommand(const std::string& name, CommandInfo info) {
    if (command_info_.count(name) && command_info_[name].implementationType == "native") {
        std::cerr << "Error: Cannot overwrite native command '" << name << "' with a configurable one. Unregister first." << std::endl;
        return false;
    }
    if (info.implementationType != "os_wrapper" && info.implementationType != "alias") {
        std::cerr << "Warning: Configurable command '" << name << "' has unsupported type: " << info.implementationType << ". Must be 'os_wrapper' or 'alias'." << std::endl;
        // Proceeding, but it might not be executable if it's not one of these and has no native func.
    }
    command_info_[name] = info;
    if (native_commands_.count(name)) { native_commands_.erase(name); } // Remove any old native placeholder
    return true;
}

bool CommandManager::unregisterCommand(const std::string& name) {
    bool removed = false;
    if (command_info_.erase(name) > 0) removed = true;
    if (native_commands_.erase(name) > 0) removed = true;
    return removed;
}

void CommandManager::parseCommandLine(const std::string& command_line, std::string& command_name, std::vector<std::string>& args) const {
    args.clear();
    std::stringstream ss(command_line);
    ss >> command_name;
    std::string arg;
    // This is a very basic parser. Shell-like parsing is complex.
    bool in_quote = false;
    std::string current_arg;
    char c;
    // Read char by char after command_name to handle quotes better
    ss.get(); // consume space after command_name if any
    while(ss.get(c)){
        if(c == '"'){
            in_quote = !in_quote;
            if(!in_quote && !current_arg.empty()){ // end of a quoted arg
                args.push_back(current_arg);
                current_arg.clear();
            }
            // else: start of quote or consecutive quote (error or intentional)
        } else if (std::isspace(c) && !in_quote){
            if(!current_arg.empty()){
                args.push_back(current_arg);
                current_arg.clear();
            }
        } else {
            current_arg += c;
        }
    }
    if(!current_arg.empty()) args.push_back(current_arg);
}


std::string CommandManager::executeCommand(const std::string& command_line) {
    std::string command_name;
    std::vector<std::string> args;
    parseCommandLine(command_line, command_name, args);

    if (command_name.empty()) return "";

    auto it_info = command_info_.find(command_name);
    if (it_info == command_info_.end()) {
        return "Error: Command '" + command_name + "' not found.";
    }
    const auto& info = it_info->second;

    for (const auto& help_arg_opt : info.help_arguments) {
        if (std::find(args.begin(), args.end(), help_arg_opt) != args.end()) {
            return getHelp(command_name);
        }
    }

    if (!info.isEnabled) {
        return "Error: Command '" + command_name + "' is currently disabled.";
    }

    try {
        if (info.implementationType == "native" && native_commands_.count(command_name)) {
            return native_commands_.at(command_name)(args);
        } else if (info.implementationType == "os_wrapper") {
            return executeOSWrapperCommand(info.osCommandTemplate, args);
        } else if (info.implementationType == "alias") {
            std::string aliased_command_line = info.osCommandTemplate;
            for(const auto& an_arg : args) aliased_command_line += " " + an_arg; // Simple append
            return executeCommand(aliased_command_line);
        } else {
            return "Error: Command '" + command_name + "' has unknown/unsupported type: " + info.implementationType;
        }
    } catch (const std::exception& e) {
        return "Error executing '" + command_name + "': " + e.what();
    }
}

std::string CommandManager::executeOSWrapperCommand(const std::string& osTemplate, const std::vector<std::string>& args) {
    std::string cmd_to_run = osTemplate;
    size_t pos_all = cmd_to_run.find("%ARGS_ALL%");
    if (pos_all != std::string::npos) {
        std::string all_args_str;
        for (size_t i = 0; i < args.size(); ++i) {
            std::string current_arg_val = args[i];
            // Basic quoting for arguments with spaces
            if (current_arg_val.find(' ') != std::string::npos && current_arg_val.front() != '"' && current_arg_val.back() != '"') {
                current_arg_val = "\"" + current_arg_val + "\"";
            }
            all_args_str += current_arg_val;
            if (i < args.size() - 1) all_args_str += " ";
        }
        cmd_to_run.replace(pos_all, std::string("%ARGS_ALL%").length(), all_args_str);
    } else {
        for (size_t i = 0; i < args.size(); ++i) {
            std::string placeholder = "%ARGS[" + std::to_string(i) + "]%";
            size_t pos_n = cmd_to_run.find(placeholder);
            if (pos_n != std::string::npos) {
                 std::string current_arg_val = args[i];
                if (current_arg_val.find(' ') != std::string::npos && current_arg_val.front() != '"' && current_arg_val.back() != '"') {
                    current_arg_val = "\"" + current_arg_val + "\"";
                }
                cmd_to_run.replace(pos_n, placeholder.length(), current_arg_val);
            }
        }
        // Remove any unreplaced %ARGS[N]% (optional, or error)
         cmd_to_run = std::regex_replace(cmd_to_run, std::regex("%ARGS\\[\\d+\\]%"), "");
    }
    return executeOSCommand_internal(cmd_to_run);
}

std::string CommandManager::getHelp(const std::string& command_name) const {
    if (!command_info_.count(command_name)) {
        return "Error: No help for '" + command_name + "'.";
    }
    const auto& info = command_info_.at(command_name);
    std::string help = "Command: " + command_name + (info.isEnabled ? "" : " (DISABLED)") + "\n";
    help += "  Description: " + info.description + "\n";
    help += "  Type: " + info.implementationType + "\n";
    if (info.implementationType == "os_wrapper" || info.implementationType == "alias") {
        help += "  Template/Target: " + info.osCommandTemplate + "\n";
    }
    help += "  Help arguments: ";
    for(const auto& ha : info.help_arguments) help += ha + " ";
    help += "\n";
    return help;
}

std::vector<std::string> CommandManager::getCommandList(bool includeDisabled) const {
    std::vector<std::string> list;
    for (const auto& [name, info] : command_info_) {
        if (includeDisabled || info.isEnabled) {
            list.push_back(name);
        }
    }
    std::sort(list.begin(), list.end());
    return list;
}

std::map<std::string, CommandInfo> CommandManager::getAllCommandInfo() const {
    return command_info_;
}

bool CommandManager::enableCommand(const std::string& name, bool enable) {
    if (command_info_.count(name)) {
        command_info_[name].isEnabled = enable;
        return true;
    }
    return false;
}

bool CommandManager::isCommandEnabled(const std::string& name) const {
    if (command_info_.count(name)) {
        return command_info_.at(name).isEnabled;
    }
    return false;
}

bool CommandManager::saveConfiguration(const std::string& filePath) const {
    std::string path_to_save = filePath.empty() ? config_file_path_ : filePath;
    if (path_to_save.empty()) {
        std::cerr << "Error: No configuration file path specified for saving." << std::endl;
        return false;
    }
    json j;
    j["commands"] = command_info_; // CommandInfo needs to be serializable (using NLOHMANN_DEFINE or manual)

    std::ofstream ofs(path_to_save);
    if (!ofs.is_open()) {
        std::cerr << "Error: Could not open file for saving: " << path_to_save << std::endl;
        return false;
    }
    try {
        ofs << j.dump(4); // Pretty print
        ofs.close();
        std::cout << "Command configuration saved to " << path_to_save << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error saving command configuration: " << e.what() << std::endl;
        if(ofs.is_open()) ofs.close();
        return false;
    }
}

bool CommandManager::loadConfiguration(const std::string& filePath) {
    std::string path_to_load = filePath.empty() ? config_file_path_ : filePath;
    if (path_to_load.empty() || !fs::exists(path_to_load)) {
        std::cerr << "Info: Command configuration file not found at '" << path_to_load << "'. Defaults will be used." << std::endl;
        return false; // Indicate that defaults should be used/initialized
    }

    std::ifstream ifs(path_to_load);
    if (!ifs.is_open()) {
        std::cerr << "Error: Could not open file for loading: " << path_to_load << std::endl;
        return false;
    }

    try {
        json j;
        ifs >> j;
        ifs.close();

        std::map<std::string, CommandInfo> loaded_command_info;
        if (j.contains("commands")) {
            // This relies on from_json for CommandInfo being defined
            loaded_command_info = j["commands"].get<std::map<std::string, CommandInfo>>();
        }

        // It's safer to initialize defaults first, then overlay loaded config
        initializeDefaultCommands(); // Get all native functions registered

        for(const auto& [name, loaded_info] : loaded_command_info) {
            if(command_info_.count(name)) { // Default native command exists
                command_info_[name].isEnabled = loaded_info.isEnabled;
                command_info_[name].description = loaded_info.description; // Allow config to override desc/help
                command_info_[name].help_arguments = loaded_info.help_arguments;
                // Don't change implementationType or osCommandTemplate if it's native from defaults
                // unless you explicitly want config to fully redefine it (which needs care).
                if (command_info_[name].implementationType == "native" &&
                    (loaded_info.implementationType == "os_wrapper" || loaded_info.implementationType == "alias")) {
                    // This means the config wants to turn a C++ func into an OS wrapper.
                    // This is okay if we unregister the native part and add as configurable.
                    // For simplicity now, we'll prefer the C++ impl if it exists by default.
                    std::cout << "Info: Command '" << name << "' has a native C++ implementation. Config definition for OS wrapper/alias ignored for this command unless native is unregistered." << std::endl;
                } else if (command_info_[name].implementationType != "native") { // If default was also os_wrapper/alias
                     command_info_[name].implementationType = loaded_info.implementationType;
                     command_info_[name].osCommandTemplate = loaded_info.osCommandTemplate;
                }
            } else { // Command is purely from config
                if (loaded_info.implementationType == "os_wrapper" || loaded_info.implementationType == "alias") {
                    addConfigurableCommand(name, loaded_info);
                } else if (loaded_info.implementationType == "native") {
                     std::cerr << "Warning: Config for '" << name << "' specifies 'native' but no C++ function is registered by default. Command may not be executable." << std::endl;
                     command_info_[name] = loaded_info; // Add metadata, but it's a stub
                } else {
                    std::cerr << "Warning: Config file contains command '" << name << "' of unknown type '"
                              << loaded_info.implementationType << "'. Skipping." << std::endl;
                }
            }
        }
        std::cout << "Command configuration loaded from " << path_to_load << std::endl;
        return true;
    } catch (const json::parse_error& e) {
        std::cerr << "Error parsing command config file '" << path_to_load << "': " << e.what() << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error loading command config from '" << path_to_load << "': " << e.what() << std::endl;
    }
    if(ifs.is_open()) ifs.close();
    return false;
}

} // namespace CommandLibrary