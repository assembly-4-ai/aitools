#ifndef COMMAND_LIBRARY_H
#define COMMAND_LIBRARY_H

#include <string>
#include <vector>
#include <map>
#include <functional> 
#include "vendor/nlohmann/json.hpp" // For JSON serialization

namespace CommandLib {

enum class CommandImplementationType {
    NATIVE_CPP,
    OS_COMMAND
};

struct CommandInfo {
    std::string name;
    std::string description;
    bool isEnabled = true;
    CommandImplementationType type = CommandImplementationType::OS_COMMAND;
    
    using NativeCppFunction = std::function<std::pair<std::string, std::string>(const std::vector<std::string>&)>;
    NativeCppFunction nativeFunction; // Only used if type is NATIVE_CPP (not serialized)

    std::string osCommandTemplate;    // Only used if type is OS_COMMAND
    bool requiresAdmin = false;

    CommandInfo() = default;

    CommandInfo(std::string n, std::string desc, std::string templ, bool enabled = true, bool admin = false)
        : name(std::move(n)), description(std::move(desc)), isEnabled(enabled),
          type(CommandImplementationType::OS_COMMAND), osCommandTemplate(std::move(templ)), requiresAdmin(admin) {}

    CommandInfo(std::string n, std::string desc, NativeCppFunction func, bool enabled = true, bool admin = false)
        : name(std::move(n)), description(std::move(desc)), isEnabled(enabled),
          type(CommandImplementationType::NATIVE_CPP), nativeFunction(std::move(func)), requiresAdmin(admin) {}
};

// Corrected NLOHMANN_JSON_SERIALIZE_ENUM invocation
// This macro is defined in the new json.hpp and should work if json.hpp is correct.
// The arguments should be pairs like {ENUM_TYPE::VALUE, "JSON_STRING_VALUE"}
NLOHMANN_JSON_SERIALIZE_ENUM(CommandImplementationType, {
    {CommandLib::CommandImplementationType::NATIVE_CPP, "NATIVE_CPP"},
    {CommandLib::CommandImplementationType::OS_COMMAND, "OS_COMMAND"}
})

// Corrected to_json and from_json for CommandInfo using nlohmann::adl_serializer pattern
// This is preferred over inline friend functions if not using NLOHMANN_DEFINE_TYPE_INTRUSIVE
namespace nlohmann {
    template <>
    struct adl_serializer<CommandLib::CommandInfo> {
        static void to_json(json& j, const CommandLib::CommandInfo& ci) {
            j = json{
                {"name", ci.name},
                {"description", ci.description},
                {"isEnabled", ci.isEnabled},
                {"type", ci.type}, // This uses the ENUM_SERIALIZE above
                {"requiresAdmin", ci.requiresAdmin}
            };
            if (ci.type == CommandLib::CommandImplementationType::OS_COMMAND) {
                j["osCommandTemplate"] = ci.osCommandTemplate;
            }
            // nativeFunction is not serialized
        }

        static void from_json(const json& j, CommandLib::CommandInfo& ci) {
            j.at("name").get_to(ci.name);
            j.at("description").get_to(ci.description);
            j.at("isEnabled").get_to(ci.isEnabled);
            j.at("type").get_to(ci.type); // This uses the ENUM_SERIALIZE above
            
            if (j.contains("requiresAdmin")) { // Check for optional field
                j.at("requiresAdmin").get_to(ci.requiresAdmin);
            } else {
                ci.requiresAdmin = false; // Default value
            }

            if (ci.type == CommandLib::CommandImplementationType::OS_COMMAND) {
                if (j.contains("osCommandTemplate")) {
                    j.at("osCommandTemplate").get_to(ci.osCommandTemplate);
                } else {
                    // OS_COMMAND type must have a template. This could be an error or default.
                    // For robustness, or from a file, it might be missing.
                    // Let CommandManager validate this if necessary before execution.
                    ci.osCommandTemplate = ""; 
                }
            }
            // nativeFunction is not deserialized, must be re-assigned programmatically
        }
    };
} // namespace nlohmann


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
