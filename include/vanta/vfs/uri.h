#pragma once

#include <filesystem>
#include <string>

namespace vanta {

class Uri {
public:
    Uri() = default;
    explicit Uri(std::string value);

    static Uri Parse(std::string value);
    static Uri FromLocalPath(const std::filesystem::path& path);

    bool Empty() const noexcept;
    const std::string& ToString() const noexcept;
    std::string Scheme() const;
    std::string Path() const;
    std::string Filename() const;
    std::string Extension() const;

    bool operator==(const Uri& other) const noexcept;
    bool operator!=(const Uri& other) const noexcept;
    bool operator<(const Uri& other) const noexcept;

private:
    std::string value_;
};

}
