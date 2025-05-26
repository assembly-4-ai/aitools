#ifndef COMMAND_LIBRARY_H
#define COMMAND_LIBRARY_H

#include <string>
#include <vector>
#include <map>
#include <functional> 
#include "vendor/nlohmann/json.hpp" // For nlohmann::json declaration

namespace CommandLib {

enum class CommandImplementationType {
    NATIVE_CPP,
    OS_COMMAND
};

// Forward declare json for use in CommandInfo methods
// Even if json.hpp is included, explicit forward declaration can sometimes help with complex templates or ensure CommandInfo is self-contained regarding types it uses.
// However, since json.hpp is included above, this might not be strictly necessary.
// namespace nlohmann { class json; } 

struct CommandInfo {
    std::string name;
    std::string description;
    bool isEnabled = true;
    CommandImplementationType type = CommandImplementationType::OS_COMMAND;
    
    using NativeCppFunction = std::function<std::pair<std::string, std::string>(const std::vector<std::string>&)>;
    NativeCppFunction nativeFunction; 

    std::string osCommandTemplate;    
    bool requiresAdmin = false;

    CommandInfo() = default;

    CommandInfo(std::string n, std::string desc, std::string templ, bool enabled = true, bool admin = false)
        : name(std::move(n)), description(std::move(desc)), isEnabled(enabled),
          type(CommandImplementationType::OS_COMMAND), osCommandTemplate(std::move(templ)), requiresAdmin(admin) {}

    CommandInfo(std::string n, std::string desc, NativeCppFunction func, bool enabled = true, bool admin = false)
        : name(std::move(n)), description(std::move(desc)), isEnabled(enabled),
          type(CommandImplementationType::NATIVE_CPP), nativeFunction(std::move(func)), requiresAdmin(admin) {}

    // New explicit serialization methods to be implemented in .cpp
    nlohmann::json toJson() const;
    static CommandInfo fromJson(const nlohmann::json& j);
};

// NLOHMANN_JSON_SERIALIZE_ENUM for CommandImplementationType is REMOVED.
// adl_serializer<CommandInfo> specialization is REMOVED.
// These will be handled by the toJson/fromJson methods in CommandInfo.

class CommandManager {
public:
    explicit CommandManager(std::string configFilePath);
    ~CommandManager() = default;

    bool registerNativeCommand(const std::string& name, const std::string& description,
                               CommandInfo::NativeCppFunction func, bool requiresAdmin = false);
    bool addConfigurableCommand(const CommandInfo& cmdInfo); 
    bool unregisterCommand(const std::string& name);

    std::pair<std::string, std::string> executeCommand(const std::string& fullCommandString);
    
    std::string getHelp(const std::string& commandName = "") const; 
    std::vector<CommandInfo> getCommandList() const;
    bool enableCommand(const std::string& name, bool enable);
    bool isCommandEnabled(const std::string& name) const;
    const CommandInfo* getCommandInfo(const std::string& name) const;

    bool loadConfiguration(); 
    bool saveConfiguration(); 

    bool checkPermissions(const CommandInfo& cmd, const std::string& userName = "defaultUser") const;

private:
    void initializeDefaultCommands(); 
    std::pair<std::string, std::string> captureOSCommandOutput(const std::string& command);
    std::vector<std::string> parseArguments(const std::string& argsString) const;
    std::string substituteArguments(std::string templateStr, const std::vector<std::string>& args) const;

    std::map<std::string, CommandInfo> m_commands;
    std::string m_configFilePath;
};

} // namespace CommandLib

#endif // COMMAND_LIBRARY_H
