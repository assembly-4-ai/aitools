#ifndef COMMAND_LIBRARY_H
#define COMMAND_LIBRARY_H

#include <string>
#include <vector>
#include <map>
#include <functional>
#include <filesystem>
#include "vendor/nlohmann/json.hpp" // Corrected path

namespace CommandLibrary {

using json = nlohmann::json;
using CommandFunction = std::function<std::string(const std::vector<std::string>& args)>;

struct CommandInfo {
    std::string description;
    std::vector<std::string> help_arguments;
    bool isEnabled = true;
    std::string implementationType = "native";
    std::string osCommandTemplate;
    // NLOHMANN_DEFINE_TYPE_INTRUSIVE(CommandInfo, description, help_arguments, isEnabled, implementationType, osCommandTemplate);
    // Manual serialization/deserialization is safer if structure changes or for more control.
};
// Manual (de)serialization for CommandInfo
void to_json(json& j, const CommandInfo& ci);
void from_json(const json& j, CommandInfo& ci);


class CommandManager {
public:
    CommandManager(const std::string& configFilePath = "commands_config.json");
    ~CommandManager();

    bool registerNativeCommand(const std::string& name, const CommandFunction& func, CommandInfo info);
    bool addConfigurableCommand(const std::string& name, CommandInfo info);
    bool unregisterCommand(const std::string& name);
    std::string executeCommand(const std::string& command_line);
    std::string getHelp(const std::string& command_name) const;
    std::vector<std::string> getCommandList(bool includeDisabled = false) const;
    std::map<std::string, CommandInfo> getAllCommandInfo() const;
    bool enableCommand(const std::string& name, bool enable = true);
    bool isCommandEnabled(const std::string& name) const;
    bool saveConfiguration(const std::string& filePath = "") const;
    bool loadConfiguration(const std::string& filePath = "");

private:
    void initializeDefaultCommands();
    std::string executeOSWrapperCommand(const std::string& osTemplate, const std::vector<std::string>& args);
    void parseCommandLine(const std::string& command_line, std::string& command_name, std::vector<std::string>& args) const;

    std::map<std::string, CommandFunction> native_commands_;
    std::map<std::string, CommandInfo> command_info_;
    std::string config_file_path_;
};

std::string executeOSCommand(const std::string& cmd_template, const std::vector<std::string>& args);
std::string executeRawOSCommand(const std::string& raw_cmd);

} // namespace CommandLibrary
#endif // COMMAND_LIBRARY_H