#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "vanta/core/event.h"
#include "vanta/core/registration.h"
#include "vanta/core/text.h"
#include "vanta/execution/job_service.h"
#include "vanta/core/value.h"
#include "vanta/vfs/virtual_file.h"

namespace vanta {

class WorkspaceContext;

enum class IndexStatus {
    Unknown,
    Indexing,
    Ready,
    Stale,
    Failed,
};

struct IndexSnapshot {
    std::string id;
    std::string kind;
    IndexStatus status = IndexStatus::Unknown;
    std::uint64_t version = 0;
    std::size_t item_count = 0;
    std::string message;
};

struct IndexChangeEvent {
    std::vector<IndexSnapshot> snapshots;
};

enum class IndexQueryKind {
    Files,
    Text,
    Symbols,
    References,
    Includes,
    Custom,
};

struct IndexQuery {
    IndexQueryKind kind = IndexQueryKind::Files;
    std::string query;
    std::size_t limit = 50;
    std::string provider_id;
};

struct IndexHit {
    VirtualFile file;
    TextRange range;
    std::string title;
    std::string preview;
    std::string provider_id;
    int score = 0;
};

struct IndexQueryResult {
    bool ok = false;
    std::string error;
    std::vector<IndexHit> hits;
};

class IndexProvider {
public:
    virtual ~IndexProvider() = default;

    virtual std::string Id() const = 0;
    virtual std::string Kind() const = 0;
    virtual IndexSnapshot Refresh(WorkspaceContext& context, JobContext& job) = 0;
    virtual bool Supports(IndexQueryKind kind) const;
    virtual IndexQueryResult Query(WorkspaceContext& context, const IndexQuery& query) const;
};

class IndexService {
public:
    RegistrationHandle RegisterProvider(std::unique_ptr<IndexProvider> provider);
    void RemoveProvider(const std::string& provider_id);
    std::vector<std::string> ProviderIds() const;

    JobId Refresh(WorkspaceContext& context, std::string title = {});
    IndexQueryResult Query(WorkspaceContext& context, const IndexQuery& query) const;
    std::vector<IndexSnapshot> Snapshots() const;
    std::optional<IndexSnapshot> Snapshot(const std::string& provider_id) const;
    void ClearSnapshots();
    std::uint64_t OnDidChangeIndex(EventBus<IndexChangeEvent>::Listener listener);
    void RemoveIndexListener(std::uint64_t listener_id);

private:
    void Publish();

    std::map<std::string, std::unique_ptr<IndexProvider>> providers_;
    std::map<std::string, IndexSnapshot> snapshots_;
    std::uint64_t next_version_ = 1;
    mutable std::mutex mutex_;
    EventBus<IndexChangeEvent> on_did_change_;
};

void RegisterDefaultIndexProviders(IndexService& indexes);
std::string ToString(IndexStatus status);
std::string ToString(IndexQueryKind kind);

}
