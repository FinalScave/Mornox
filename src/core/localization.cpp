#include "vanta/core/localization.h"

#include <fstream>
#include <sstream>
#include <utility>

namespace vanta {
namespace {

std::string Trim(std::string value) {
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return {};
    }
    const auto last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

std::string UnescapePropertiesValue(const std::string& value) {
    std::string result;
    result.reserve(value.size());
    bool escaped = false;
    for (char ch : value) {
        if (!escaped) {
            if (ch == '\\') {
                escaped = true;
            } else {
                result.push_back(ch);
            }
            continue;
        }

        switch (ch) {
        case 'n':
            result.push_back('\n');
            break;
        case 't':
            result.push_back('\t');
            break;
        case 'r':
            result.push_back('\r');
            break;
        default:
            result.push_back(ch);
            break;
        }
        escaped = false;
    }
    if (escaped) {
        result.push_back('\\');
    }
    return result;
}

std::string ValueToDisplayString(const Value& value) {
    if (value.IsNull()) {
        return "null";
    }
    if (value.IsBool()) {
        return value.AsBool() ? "true" : "false";
    }
    if (value.IsInt()) {
        return std::to_string(value.AsInt());
    }
    if (value.IsDouble()) {
        std::ostringstream stream;
        stream << value.AsDouble();
        return stream.str();
    }
    if (value.IsString()) {
        return value.AsString();
    }
    return {};
}

std::string EffectiveLocale(const std::string& locale, const std::string& default_locale) {
    return locale.empty() ? default_locale : locale;
}

}

Localizer::Localizer(const LocalizationRegistry& registry, std::string owner_id)
    : registry_(&registry), owner_id_(std::move(owner_id)) {}

LocalizedText Localizer::Text(std::string key, std::vector<Value> args) const {
    return {
        .owner_id = owner_id_,
        .key = std::move(key),
        .args = std::move(args),
    };
}

std::string Localizer::Resolve(std::string key, std::vector<Value> args, std::string locale) const {
    return Resolve(Text(std::move(key), std::move(args)), std::move(locale));
}

std::string Localizer::Resolve(const LocalizedText& text, std::string locale) const {
    if (registry_ == nullptr) {
        return InterpolateLocalizedPattern(text.key, text.args);
    }
    return registry_->Resolve(text, locale);
}

LocalizationRegistry::LocalizationRegistry(std::string default_locale)
    : default_locale_(std::move(default_locale)) {}

const std::string& LocalizationRegistry::DefaultLocale() const {
    return default_locale_;
}

void LocalizationRegistry::SetDefaultLocale(std::string locale) {
    default_locale_ = std::move(locale);
}

RegistrationHandle LocalizationRegistry::RegisterCatalog(LocalizationCatalog catalog) {
    if (catalog.owner_id.empty() || catalog.locale.empty()) {
        return {};
    }

    const std::string owner_id = catalog.owner_id;
    const std::string locale = catalog.locale;
    catalogs_[owner_id][locale] = std::move(catalog.messages);
    return RegistrationHandle([this, owner_id, locale] {
        RemoveCatalog(owner_id, locale);
    });
}

Localizer LocalizationRegistry::LocalizerForOwner(std::string owner_id) const {
    return Localizer(*this, std::move(owner_id));
}

std::optional<std::string> LocalizationRegistry::Message(const std::string& owner_id, const std::string& locale, const std::string& key) const {
    auto owner = catalogs_.find(owner_id);
    if (owner == catalogs_.end()) {
        return std::nullopt;
    }
    auto catalog = owner->second.find(locale);
    if (catalog == owner->second.end()) {
        return std::nullopt;
    }
    auto message = catalog->second.find(key);
    return message == catalog->second.end() ? std::nullopt : std::optional<std::string>(message->second);
}

std::string LocalizationRegistry::Resolve(const LocalizedText& text, const std::string& locale) const {
    const std::string requested_locale = EffectiveLocale(locale, default_locale_);
    std::optional<std::string> pattern = Message(text.owner_id, requested_locale, text.key);
    if (!pattern && requested_locale != default_locale_) {
        pattern = Message(text.owner_id, default_locale_, text.key);
    }
    return InterpolateLocalizedPattern(pattern.value_or(text.key), text.args);
}

void LocalizationRegistry::RemoveCatalog(const std::string& owner_id, const std::string& locale) {
    auto owner = catalogs_.find(owner_id);
    if (owner == catalogs_.end()) {
        return;
    }
    owner->second.erase(locale);
    if (owner->second.empty()) {
        catalogs_.erase(owner);
    }
}

Result<LocalizationCatalog> ParseLocalizationProperties(
    std::string owner_id,
    std::string locale,
    const std::string& text) {
    LocalizationCatalog catalog{
        .owner_id = std::move(owner_id),
        .locale = std::move(locale),
    };

    std::istringstream input(text);
    std::string line;
    int line_number = 0;
    while (std::getline(input, line)) {
        ++line_number;
        std::string trimmed = Trim(line);
        if (trimmed.empty() || trimmed.starts_with('#') || trimmed.starts_with('!')) {
            continue;
        }

        const auto separator = trimmed.find('=');
        if (separator == std::string::npos) {
            return Result<LocalizationCatalog>::Failure(
                "localization.invalidProperty",
                "Invalid localization property at line " + std::to_string(line_number));
        }

        std::string key = Trim(trimmed.substr(0, separator));
        std::string value = Trim(trimmed.substr(separator + 1));
        if (key.empty()) {
            return Result<LocalizationCatalog>::Failure(
                "localization.emptyKey",
                "Empty localization key at line " + std::to_string(line_number));
        }
        catalog.messages[std::move(key)] = UnescapePropertiesValue(value);
    }

    return Result<LocalizationCatalog>::Success(std::move(catalog));
}

Result<LocalizationCatalog> ReadLocalizationProperties(
    std::string owner_id,
    std::string locale,
    const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input) {
        return Result<LocalizationCatalog>::Failure(
            "localization.readFailed",
            "Failed to read localization file: " + path.string());
    }
    std::ostringstream stream;
    stream << input.rdbuf();
    return ParseLocalizationProperties(std::move(owner_id), std::move(locale), stream.str());
}

std::string InterpolateLocalizedPattern(const std::string& pattern, const std::vector<Value>& args) {
    std::string result;
    result.reserve(pattern.size());
    std::size_t arg_index = 0;
    for (std::size_t i = 0; i < pattern.size(); ++i) {
        const char ch = pattern[i];
        if (ch == '{' && i + 1 < pattern.size() && pattern[i + 1] == '{') {
            result.push_back('{');
            ++i;
        } else if (ch == '}' && i + 1 < pattern.size() && pattern[i + 1] == '}') {
            result.push_back('}');
            ++i;
        } else if (ch == '{' && i + 1 < pattern.size() && pattern[i + 1] == '}') {
            if (arg_index < args.size()) {
                result += ValueToDisplayString(args[arg_index]);
                ++arg_index;
            } else {
                result += "{}";
            }
            ++i;
        } else {
            result.push_back(ch);
        }
    }
    return result;
}

}
