#pragma once

#include <cstdint>

#include "vanta/workspace/document_service.h"
#include "vanta/language/language_service.h"

namespace vanta {

class DocumentLanguageSynchronizer {
public:
    DocumentLanguageSynchronizer(DocumentService& documents, LanguageRegistry& languages);
    ~DocumentLanguageSynchronizer();

    void Start();
    void Stop();

private:
    void HandleChange(const DocumentChangeEvent& event);

    DocumentService& documents_;
    LanguageRegistry& languages_;
    std::uint64_t listener_id_ = 0;
};

}
