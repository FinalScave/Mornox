#pragma once

#include <cstdint>

#include "mornox/workspace/document_service.h"
#include "mornox/language/language_service.h"

namespace mornox {

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
