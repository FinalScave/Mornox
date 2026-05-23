#pragma once

#include <filesystem>
#include <string>

namespace mornox {

struct GitDiff {
    int exit_code = -1;
    std::string text;
};

class GitService {
public:
    static constexpr const char* kServiceId = "mornox.git";

    virtual ~GitService() = default;

    virtual GitDiff Diff() const = 0;
};

}
