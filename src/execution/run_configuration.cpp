#include "execution/run_configuration_registry_impl.h"

#include <exception>
#include <filesystem>
#include <future>
#include <memory>
#include <sstream>
#include <utility>

#include "core/value_projection.h"
#include "vanta/core/json_codec.h"
#include "vanta/project/project.h"
#include "vanta/project/project_manager.h"
#include "vanta/workspace/workspace_context.h"
#include "vanta/workspace/workspace_runtime.h"

namespace vanta {
namespace {

ValidationResult Missing(const std::string &message) {
  return {
      .ok = false,
      .messages = {message},
  };
}

std::vector<std::string> StringsFromValue(const Value& value) {
  std::vector<std::string> values;
  if (!value.IsArray()) {
    return values;
  }
  for (const Value& item : value.AsArray()) {
    if (item.IsString()) {
      values.push_back(item.AsString());
    }
  }
  return values;
}

Value StringsProjection(const std::vector<std::string>& values) {
  Value::Array array;
  for (const std::string& value : values) {
    array.push_back(Value(value));
  }
  return Value::ArrayValue(std::move(array));
}

const CustomCommandRunConfigurationPayload* CustomCommandPayload(const RunConfiguration& configuration) {
  return dynamic_cast<const CustomCommandRunConfigurationPayload*>(configuration.payload.get());
}

Value CustomCommandPayloadProjection(const CustomCommandRunConfigurationPayload& payload) {
  return Value::ObjectValue({
      {"executable", Value(payload.executable)},
      {"arguments", StringsProjection(payload.arguments)},
      {"workingDirectory", Value(payload.working_directory.string())},
  });
}

Value RunConfigurationState(const RunConfiguration& configuration, const RunConfigurationType* type) {
  return Value::ObjectValue({
      {"id", Value(configuration.id)},
      {"name", Value(configuration.name)},
      {"typeId", Value(configuration.type_id)},
      {"targetId", Value(configuration.target_id)},
      {"data", type == nullptr || configuration.payload == nullptr ? Value::ObjectValue() : type->SerializePayload(*configuration.payload)},
      {"temporary", Value(configuration.temporary)},
  });
}

class CustomCommandRunConfigurationType final : public RunConfigurationType {
public:
  std::string Id() const override { return "custom.command"; }

  std::string Title() const override { return "Custom Command"; }

  std::unique_ptr<RunConfigurationPayload> DefaultPayload(WorkspaceContext &context) const override {
    auto payload = std::make_unique<CustomCommandRunConfigurationPayload>();
    payload->working_directory = context.CurrentWorkspace().Info().root_path;
    return payload;
  }

  std::unique_ptr<RunConfigurationPayload> DeserializePayload(const Value& value) const override {
    return CustomCommandRunConfigurationPayload::FromValue(value);
  }

  Value SerializePayload(const RunConfigurationPayload& payload) const override {
    const auto* custom_payload = dynamic_cast<const CustomCommandRunConfigurationPayload*>(&payload);
    return custom_payload == nullptr ? Value::ObjectValue() : CustomCommandPayloadProjection(*custom_payload);
  }

  std::vector<ConfigurationField> Fields() const override {
    return {
        {.id = "executable",
         .title = "Executable",
         .type = "string",
         .default_value = Value(""),
         .required = true},
        {.id = "arguments",
         .title = "Arguments",
         .type = "stringArray",
         .default_value = Value::ArrayValue(),
         .required = false},
        {.id = "workingDirectory",
         .title = "Working Directory",
         .type = "path",
         .default_value = Value(""),
         .required = false},
    };
  }

  ValidationResult
  Validate(WorkspaceContext &context,
           const RunConfiguration &configuration) const override {
    (void)context;
    const CustomCommandRunConfigurationPayload* payload = CustomCommandPayload(configuration);
    if (payload == nullptr || payload->executable.empty()) {
      return Missing("Executable is required");
    }
    return {};
  }

  RunResult Run(RunExecutionContext &context,
                const RunConfiguration &configuration) const override {
    const CustomCommandRunConfigurationPayload* payload = CustomCommandPayload(configuration);
    if (payload == nullptr) {
      return {.exit_code = -1, .output = "Run configuration payload is invalid\n", .job_id = context.job_id};
    }
    std::filesystem::path working_directory = payload->working_directory;
    if (working_directory.empty()) {
      working_directory = context.workspace.CurrentWorkspace().Info().root_path;
    }

    const ExecutionResult execution = context.workspace.Execution().Execute(
        context.workspace,
        {
            .executable = payload->executable,
            .arguments = payload->arguments,
            .working_directory = working_directory,
            .job_id = context.job_id,
        },
        context.target);
    return {
        .exit_code = execution.exit_code,
        .output = execution.output,
        .diagnostics = {},
        .job_id = context.job_id,
    };
  }
};

std::optional<RunConfiguration> ConfigurationFromValue(const Value& item, RunConfigurationType* type) {
  if (!item.IsObject()) {
    return std::nullopt;
  }
  RunConfiguration configuration;
  configuration.id = item.StringValue("id").value_or("");
  configuration.name = item.StringValue("name").value_or(configuration.id);
  configuration.type_id = item.StringValue("typeId").value_or("");
  configuration.target_id = item.StringValue("targetId").value_or("");
  if (type != nullptr) {
    configuration.payload = type->DeserializePayload(item.Contains("data") ? item["data"] : Value::ObjectValue());
  }
  configuration.temporary = item.BoolValue("temporary").value_or(configuration.temporary);
  if (configuration.id.empty() || configuration.type_id.empty()) {
    return std::nullopt;
  }
  if (configuration.payload == nullptr) {
    return std::nullopt;
  }
  return configuration;
}

} // namespace

std::unique_ptr<RunConfigurationPayload> CustomCommandRunConfigurationPayload::Clone() const {
  return std::make_unique<CustomCommandRunConfigurationPayload>(*this);
}

std::unique_ptr<RunConfigurationPayload> CustomCommandRunConfigurationPayload::FromValue(const Value& value) {
  auto payload = std::make_unique<CustomCommandRunConfigurationPayload>();
  if (!value.IsObject()) {
    return payload;
  }
  payload->executable = value.StringValue("executable").value_or("");
  if (value.Contains("arguments")) {
    payload->arguments = StringsFromValue(value["arguments"]);
  }
  if (auto working_directory = value.StringValue("workingDirectory")) {
    payload->working_directory = *working_directory;
  }
  return payload;
}

RunConfiguration::RunConfiguration(const RunConfiguration& other)
    : id(other.id),
      name(other.name),
      type_id(other.type_id),
      target_id(other.target_id),
      payload(other.payload == nullptr ? nullptr : other.payload->Clone()),
      temporary(other.temporary) {}

RunConfiguration& RunConfiguration::operator=(const RunConfiguration& other) {
  if (this == &other) {
    return *this;
  }
  id = other.id;
  name = other.name;
  type_id = other.type_id;
  target_id = other.target_id;
  payload = other.payload == nullptr ? nullptr : other.payload->Clone();
  temporary = other.temporary;
  return *this;
}

RegistrationHandle internal::RunConfigurationRegistryImpl::RegisterType(
    std::unique_ptr<RunConfigurationType> type) {
  if (type == nullptr || type->Id().empty()) {
    return {};
  }
  const std::string id = type->Id();
  types_[id] = std::move(type);
  return RegistrationHandle([this, id] {
    RemoveType(id);
  });
}

void internal::RunConfigurationRegistryImpl::RemoveType(const std::string &type_id) {
  types_.erase(type_id);
}

RunConfigurationType *
internal::RunConfigurationRegistryImpl::Type(const std::string &type_id) const {
  auto it = types_.find(type_id);
  return it == types_.end() ? nullptr : it->second.get();
}

std::vector<std::string> internal::RunConfigurationRegistryImpl::TypeIds() const {
  std::vector<std::string> ids;
  for (const auto &[id, type] : types_) {
    (void)type;
    ids.push_back(id);
  }
  return ids;
}

RegistrationHandle internal::RunConfigurationRegistryImpl::RegisterProducer(
    std::unique_ptr<RunConfigurationProducer> producer) {
  if (producer == nullptr || producer->Id().empty()) {
    return {};
  }
  const std::string id = producer->Id();
  producers_[id] = std::move(producer);
  return RegistrationHandle([this, id] {
    RemoveProducer(id);
  });
}

void internal::RunConfigurationRegistryImpl::RemoveProducer(const std::string &producer_id) {
  producers_.erase(producer_id);
}

std::vector<std::string> internal::RunConfigurationRegistryImpl::ProducerIds() const {
  std::vector<std::string> ids;
  for (const auto &[id, producer] : producers_) {
    (void)producer;
    ids.push_back(id);
  }
  return ids;
}

std::vector<RunConfiguration>
internal::RunConfigurationRegistryImpl::Produce(WorkspaceContext &context,
                                 const VirtualFile &focus_file) const {
  std::vector<RunConfiguration> values;
  for (const auto &[id, producer] : producers_) {
    (void)id;
    std::vector<RunConfiguration> produced =
        producer->Produce(context, focus_file);
    values.insert(values.end(), produced.begin(), produced.end());
  }
  return values;
}

RunResult internal::RunConfigurationRegistryImpl::Run(WorkspaceContext &context,
                                       const std::string &configuration_id,
                                       const std::string &target_id) const {
  const Project* project = context.CurrentProject();
  const ProjectRunConfigurations* configurations =
      project == nullptr ? nullptr : project->GetComponent<ProjectRunConfigurations>(ProjectRunConfigurations::kComponentId);
  const auto configuration_value =
      configurations == nullptr ? std::optional<RunConfiguration>() : configurations->Configuration(configuration_id);
  if (!configuration_value) {
    return {.exit_code = -1, .output = "Run configuration not found\n"};
  }
  const RunConfiguration &configuration = *configuration_value;
  RunConfigurationType *type_value = Type(configuration.type_id);
  if (type_value == nullptr) {
    return {.exit_code = -1, .output = "Run configuration type not found\n"};
  }

  const ValidationResult validation =
      type_value->Validate(context, configuration);
  if (!validation.ok) {
    std::ostringstream output;
    for (const std::string &message : validation.messages) {
      output << message << '\n';
    }
    return {.exit_code = -1, .output = output.str()};
  }

  std::vector<ExecutionTarget> available_targets = context.Execution().Targets(context);
  const std::string resolved_target_id =
      target_id.empty() ? configuration.target_id : target_id;
  auto target_it = available_targets.begin();
  if (!resolved_target_id.empty()) {
    target_it = std::find_if(available_targets.begin(), available_targets.end(),
                            [&](const ExecutionTarget &target) {
                              return target.id == resolved_target_id;
                            });
  }
  if (target_it == available_targets.end()) {
    return {.exit_code = -1, .output = "Run target not found\n"};
  }

  auto result_promise = std::make_shared<std::promise<RunResult>>();
  std::future<RunResult> result_future = result_promise->get_future();
  const ExecutionTarget target = *target_it;
  JobHandle handle = context.Jobs().Submit(
      context.Async(),
      {
          .kind = JobKind::Run,
          .title = configuration.name.empty() ? configuration.id
                                              : configuration.name,
      },
      [&context, type_value, configuration, target,
       result_promise](JobContext &job) mutable {
        try {
          job.Report(0.1, "Run started");
          RunExecutionContext execution_context{
              .workspace = context,
              .job_id = job.Id(),
              .target = target,
          };
          RunResult result = type_value->Run(execution_context, configuration);
          result.job_id = job.Id();
          if (!context.Jobs().IsTerminal(job.Id()) && !result.output.empty()) {
            job.AppendOutput(result.output);
          }
          result_promise->set_value(result);
          return JobResult{
              .success = result.exit_code == 0,
              .payload = internal::RunResultProjection(result),
          };
        } catch (const std::exception &error) {
          RunResult result{
              .exit_code = -1,
              .output = std::string(error.what()) + "\n",
              .job_id = job.Id(),
          };
          result_promise->set_value(result);
          return JobResult{
              .success = false,
              .message = error.what(),
              .payload = internal::RunResultProjection(result),
          };
        } catch (...) {
          RunResult result{
              .exit_code = -1,
              .output = "Run failed\n",
              .job_id = job.Id(),
          };
          result_promise->set_value(result);
          return JobResult{
              .success = false,
              .message = "Run failed",
              .payload = internal::RunResultProjection(result),
          };
        }
      });
  RunResult result = result_future.get();
  handle.Wait();
  return result;
}

std::string ProjectRunConfigurations::Id() const { return kComponentId; }

void ProjectRunConfigurations::OnAttach(WorkspaceContext& context) {
  context_ = &context;
}

void ProjectRunConfigurations::RestoreState(const Value& state) {
  configurations_.clear();
  if (!state.IsObject() || !state.Contains("configurations") ||
      !state["configurations"].IsArray()) {
    return;
  }
  for (const Value& item : state["configurations"].AsArray()) {
    const std::string type_id = item.IsObject() ? item.StringValue("typeId").value_or("") : "";
    RunConfigurationType* type = context_ == nullptr ? nullptr : context_->RunConfigurations().Type(type_id);
    if (auto configuration = ConfigurationFromValue(item, type)) {
      configuration->temporary = false;
      AddConfiguration(std::move(*configuration));
    }
  }
}

Value ProjectRunConfigurations::SaveState() const {
  Value::Array configurations;
  for (const RunConfiguration &configuration : this->Configurations(false)) {
    RunConfigurationType* type = context_ == nullptr ? nullptr : context_->RunConfigurations().Type(configuration.type_id);
    configurations.push_back(RunConfigurationState(configuration, type));
  }
  return Value::ObjectValue({
      {"schemaVersion", Value(static_cast<std::int64_t>(1))},
      {"configurations", Value::ArrayValue(std::move(configurations))},
  });
}

void ProjectRunConfigurations::AddConfiguration(RunConfiguration configuration) {
  if (configuration.id.empty() || configuration.type_id.empty()) {
    return;
  }
  if (configuration.payload == nullptr && context_ != nullptr) {
    if (RunConfigurationType* type = context_->RunConfigurations().Type(configuration.type_id)) {
      configuration.payload = type->DefaultPayload(*context_);
    }
  }
  if (configuration.payload == nullptr) {
    return;
  }
  configurations_[configuration.id] = std::move(configuration);
}

RegistrationHandle ProjectRunConfigurations::RegisterConfiguration(RunConfiguration configuration) {
  if (configuration.id.empty()) {
    return {};
  }
  const std::string id = configuration.id;
  AddConfiguration(std::move(configuration));
  return RegistrationHandle([this, id] {
    RemoveConfiguration(id);
  });
}

bool ProjectRunConfigurations::RemoveConfiguration(
    const std::string &configuration_id) {
  return configurations_.erase(configuration_id) > 0;
}

std::optional<RunConfiguration> ProjectRunConfigurations::Configuration(
    const std::string &configuration_id) const {
  auto it = configurations_.find(configuration_id);
  return it == configurations_.end()
             ? std::nullopt
             : std::optional<RunConfiguration>(it->second);
}

std::vector<RunConfiguration>
ProjectRunConfigurations::Configurations(bool include_temporary) const {
  std::vector<RunConfiguration> values;
  for (const auto &[id, configuration] : configurations_) {
    (void)id;
    if (include_temporary || !configuration.temporary) {
      values.push_back(configuration);
    }
  }
  return values;
}

void ProjectRunConfigurations::SetConfigurations(
    std::vector<RunConfiguration> configurations) {
  configurations_.clear();
  for (RunConfiguration &configuration : configurations) {
    AddConfiguration(std::move(configuration));
  }
}

void RegisterDefaultRunConfigurationProviders(
    RunConfigurationRegistry &catalog) {
  catalog.RegisterType(std::make_unique<CustomCommandRunConfigurationType>());
}

} // namespace vanta
