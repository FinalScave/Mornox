#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "vanta/core/diagnostic.h"
#include "vanta/workspace/document_service.h"
#include "vanta/core/registration.h"
#include "vanta/core/text.h"
#include "vanta/core/value.h"

namespace vanta {

struct ProjectModel;

struct TextDocumentIdentifier {
    VirtualFile file;
    std::string language_id;
};

struct TextDocumentPosition {
    TextDocumentIdentifier document;
    TextPosition position;
};

class LanguageService;

struct BracketPair {
    std::string open;
    std::string close;
};

struct LanguageDefinition {
    std::string display_name;
    std::vector<std::string> aliases;
};

struct LanguageAssociation {
    std::vector<std::string> extensions;
    std::vector<std::string> filenames;
    std::vector<std::string> glob_patterns;
};

struct LanguageConfiguration {
    std::string line_comment;
    std::vector<std::string> block_comment;
    std::vector<BracketPair> brackets;
    std::vector<BracketPair> auto_closing_pairs;
    std::vector<BracketPair> surrounding_pairs;
    std::vector<std::string> word_pattern;
};

struct LanguageSelector {
    std::vector<std::string> project_facets;
    std::vector<std::string> capabilities;
};

struct LanguageResolutionContext {
    const ProjectModel* project = nullptr;
    std::string capability;
};

struct Language {
    std::string id;
    LanguageDefinition definition;
    LanguageAssociation association;
    LanguageConfiguration configuration;
    LanguageSelector selector;
    LanguageService* service = nullptr;
    int priority = 0;
};

struct CompletionItem {
    std::string label;
    std::string insert_text;
    std::string detail;
    std::string documentation;
};

struct LanguageRequestTrace {
    int id = 0;
    std::string method;
    std::string raw_request;
    std::string raw_response;
};

struct CompletionList {
    bool ok = false;
    std::string error;
    bool incomplete = false;
    std::vector<CompletionItem> items;
    LanguageRequestTrace trace;
};

struct HoverResult {
    bool ok = false;
    std::string error;
    std::string contents;
    LanguageRequestTrace trace;
};

struct Location {
    VirtualFile file;
    TextRange range;
};

struct LocationResult {
    bool ok = false;
    std::string error;
    std::vector<Location> locations;
    LanguageRequestTrace trace;
};

struct SemanticTokens {
    bool ok = false;
    std::string error;
    std::vector<std::int64_t> data;
    LanguageRequestTrace trace;
};

class LanguageService {
public:
    virtual ~LanguageService() = default;

    virtual bool Start(std::string* error_message = nullptr) = 0;
    virtual bool Running() const = 0;
    virtual void Stop() = 0;

    virtual void DidOpen(const TextDocument& document);
    virtual void DidChange(const TextDocument& document);
    virtual void DidSave(const TextDocument& document);
    virtual void DidClose(const VirtualFile& file);

    virtual CompletionList Completion(const TextDocumentPosition& request) = 0;
    virtual HoverResult Hover(const TextDocumentPosition& request) = 0;
    virtual LocationResult Definition(const TextDocumentPosition& request) = 0;
    virtual SemanticTokens SemanticTokensFull(const TextDocumentIdentifier& document) = 0;
};

class LanguageRegistry {
public:
    virtual ~LanguageRegistry() = default;

    virtual RegistrationHandle RegisterLanguage(Language language) = 0;
    virtual std::vector<Language> Languages() const = 0;
    virtual const Language* LanguageForFile(const VirtualFile& file) const = 0;
    virtual const Language* LanguageForFile(const VirtualFile& file, const LanguageResolutionContext& context) const = 0;
    virtual const Language* LanguageForId(const std::string& language_id) const = 0;
    virtual const Language* LanguageForId(const std::string& language_id, const LanguageResolutionContext& context) const = 0;
    virtual LanguageService* ServiceForLanguage(const std::string& language_id) const = 0;
    virtual LanguageService* ServiceForLanguage(const std::string& language_id, const LanguageResolutionContext& context) const = 0;
    virtual LanguageService* ServiceForDocument(const VirtualFile& file) const = 0;
    virtual LanguageService* ServiceForDocument(const VirtualFile& file, const LanguageResolutionContext& context) const = 0;
    virtual std::string LanguageIdForFile(const VirtualFile& file) const = 0;
    virtual std::string LanguageIdForFile(const VirtualFile& file, const LanguageResolutionContext& context) const = 0;
    virtual std::vector<std::string> LanguageIds() const = 0;
};

std::vector<Language> DefaultLanguages();
void RegisterDefaultLanguages(LanguageRegistry& languages);

}
