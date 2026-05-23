#pragma once

#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace mornox {

struct CommandResult {
    int exit_code = -1;
    std::string standard_output;
    std::string standard_error;
};

struct CommandSpec {
    std::string executable;
    std::vector<std::string> arguments;
    std::filesystem::path working_directory;
};

struct CommandCallbacks {
    std::function<void(const std::string&)> on_stdout;
    std::function<void(const std::string&)> on_stderr;
};

CommandResult RunCommand(const CommandSpec& spec, CommandCallbacks callbacks = {});

class ChildProcess {
public:
    ChildProcess() = default;
    ChildProcess(const ChildProcess&) = delete;
    ChildProcess& operator=(const ChildProcess&) = delete;
    ChildProcess(ChildProcess&& other) noexcept;
    ChildProcess& operator=(ChildProcess&& other) noexcept;
    ~ChildProcess();

    bool Start(const CommandSpec& spec, std::string* error_message = nullptr);
    bool Running() const;
    std::optional<int> TryWait();
    int Wait();
    bool WriteStdin(const std::string& text);
    std::string ReadStdoutAvailable();
    std::string ReadStderrAvailable();
    void Terminate();
    std::optional<int> ExitCode() const;

private:
    void ClosePipes();
    void RememberExitStatus(int status);

    int pid_ = -1;
    int stdin_fd_ = -1;
    int stdout_fd_ = -1;
    int stderr_fd_ = -1;
    std::optional<int> exit_code_;
};

}
