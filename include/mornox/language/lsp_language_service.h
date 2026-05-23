#pragma once

#include <filesystem>
#include <string>

#include "mornox/language/language_service.h"
#include "mornox/language/lsp_client.h"

namespace mornox {

class LspLanguageService final : public LanguageService {
public:
    LspLanguageService(std::filesystem::path server_path, std::filesystem::path workspace_root, std::string language_id = {});

    bool Start(std::string* error_message = nullptr) override;
    bool Running() const override;
    void Stop() override;

    void DidOpen(const TextDocument& document) override;
    void DidChange(const TextDocument& document) override;
    void DidSave(const TextDocument& document) override;
    void DidClose(const VirtualFile& file) override;

    CompletionList Completion(const TextDocumentPosition& request) override;
    HoverResult Hover(const TextDocumentPosition& request) override;
    LocationResult Definition(const TextDocumentPosition& request) override;
    SemanticTokens SemanticTokensFull(const TextDocumentIdentifier& document) override;

private:
    std::filesystem::path server_path_;
    std::filesystem::path workspace_root_;
    std::string language_id_;
    LspClient client_;
};

}
