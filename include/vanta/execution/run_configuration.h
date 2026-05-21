#pragma once

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "vanta/core/registration.h"
#include "vanta/core/value.h"
#include "vanta/workspace/diagnostic_service.h"
#include "vanta/execution/execution_service.h"
#include "vanta/project/component.h"
#include "vanta/vfs/virtual_file.h"

namespace vanta {

class WorkspaceContext;
struct ExecutionTarget;

struct ConfigurationField {
    std::string id;
    std::string title;
    std::string type;
    Value default_value;
    bool required = false;
};

struct ValidationResult {
    bool ok = true;
    std::vector<std::string> messages;
};

class RunConfigurationPayload {
public:
    virtual ~RunConfigurationPayload() = default;

    virtual std::unique_ptr<RunConfigurationPayload> Clone() const = 0;
};

struct RunConfiguration {
    RunConfiguration() = default;
    RunConfiguration(const RunConfiguration& other);
    RunConfiguration& operator=(const RunConfiguration& other);
    RunConfiguration(RunConfiguration&&) noexcept = default;
    RunConfiguration& operator=(RunConfiguration&&) noexcept = default;
    ~RunConfiguration() = default;

    std::string id;
    std::string name;
    std::string type_id;
    std::string target_id;
    std::unique_ptr<RunConfigurationPayload> payload;
    bool temporary = false;
};

class CustomCommandRunConfigurationPayload final : public RunConfigurationPayload {
public:
    std::string executable;
    std::vector<std::string> arguments;
    std::filesystem::path working_directory;

    std::unique_ptr<RunConfigurationPayload> Clone() const override;
    static std::unique_ptr<RunConfigurationPayload> FromValue(const Value& value);
};

struct RunResult {
    int exit_code = -1;
    std::string output;
    std::vector<Diagnostic> diagnostics;
    JobId job_id = 0;
};

struct RunExecutionContext {
    WorkspaceContext& workspace;
    JobId job_id = 0;
    ExecutionTarget target;
};

class RunConfigurationType {
public:
    virtual ~RunConfigurationType() = default;

    virtual std::string Id() const = 0;
    virtual std::string Title() const = 0;
    virtual std::unique_ptr<RunConfigurationPayload> DefaultPayload(WorkspaceContext& context) const = 0;
    virtual std::unique_ptr<RunConfigurationPayload> DeserializePayload(const Value& value) const = 0;
    virtual Value SerializePayload(const RunConfigurationPayload& payload) const = 0;
    virtual std::vector<ConfigurationField> Fields() const = 0;
    virtual ValidationResult Validate(WorkspaceContext& context, const RunConfiguration& configuration) const = 0;
    virtual RunResult Run(RunExecutionContext& context, const RunConfiguration& configuration) const = 0;
};

class RunConfigurationProducer {
public:
    virtual ~RunConfigurationProducer() = default;

    virtual std::string Id() const = 0;
    virtual std::vector<RunConfiguration> Produce(WorkspaceContext& context, const VirtualFile& focus_file) const = 0;
};

class RunConfigurationRegistry {
public:
    virtual ~RunConfigurationRegistry() = default;

    virtual RegistrationHandle RegisterType(std::unique_ptr<RunConfigurationType> type) = 0;
    virtual void RemoveType(const std::string& type_id) = 0;
    virtual RunConfigurationType* Type(const std::string& type_id) const = 0;
    virtual std::vector<std::string> TypeIds() const = 0;

    virtual RegistrationHandle RegisterProducer(std::unique_ptr<RunConfigurationProducer> producer) = 0;
    virtual void RemoveProducer(const std::string& producer_id) = 0;
    virtual std::vector<std::string> ProducerIds() const = 0;

    virtual std::vector<RunConfiguration> Produce(WorkspaceContext& context, const VirtualFile& focus_file) const = 0;
    virtual RunResult Run(WorkspaceContext& context, const std::string& configuration_id, const std::string& target_id = "") const = 0;
};

class ProjectRunConfigurations final : public Component {
public:
    static constexpr const char* kComponentId = "vanta.project.runConfigurations";

    std::string Id() const override;
    void OnAttach(WorkspaceContext& context) override;
    void RestoreState(const Value& state) override;
    Value SaveState() const override;

    void AddConfiguration(RunConfiguration configuration);
    RegistrationHandle RegisterConfiguration(RunConfiguration configuration);
    bool RemoveConfiguration(const std::string& configuration_id);
    std::optional<RunConfiguration> Configuration(const std::string& configuration_id) const;
    std::vector<RunConfiguration> Configurations(bool include_temporary = false) const;
    void SetConfigurations(std::vector<RunConfiguration> configurations);

private:
    WorkspaceContext* context_ = nullptr;
    std::map<std::string, RunConfiguration> configurations_;
};

void RegisterDefaultRunConfigurationProviders(RunConfigurationRegistry& catalog);

}
