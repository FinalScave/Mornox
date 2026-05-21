#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "vanta/execution/run_configuration.h"

namespace vanta::internal {

class RunConfigurationRegistryImpl final : public RunConfigurationRegistry {
public:
    RegistrationHandle RegisterType(std::unique_ptr<RunConfigurationType> type) override;
    void RemoveType(const std::string& type_id) override;
    RunConfigurationType* Type(const std::string& type_id) const override;
    std::vector<std::string> TypeIds() const override;

    RegistrationHandle RegisterProducer(std::unique_ptr<RunConfigurationProducer> producer) override;
    void RemoveProducer(const std::string& producer_id) override;
    std::vector<std::string> ProducerIds() const override;

    std::vector<RunConfiguration> Produce(WorkspaceContext& context, const VirtualFile& focus_file) const override;
    RunResult Run(WorkspaceContext& context, const std::string& configuration_id, const std::string& target_id = "") const override;

private:
    std::map<std::string, std::unique_ptr<RunConfigurationType>> types_;
    std::map<std::string, std::unique_ptr<RunConfigurationProducer>> producers_;
};

}
