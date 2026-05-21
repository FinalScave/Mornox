#include "vanta/agent/agent_operation.h"

#include <cstdint>
#include <exception>
#include <future>
#include <memory>
#include <optional>
#include <utility>

#include "core/value_projection.h"
#include "vanta/agent/agent_tool_registry.h"
#include "vanta/execution/job_service.h"
#include "vanta/core/json_codec.h"
#include "vanta/plugin/approval_service.h"
#include "vanta/workspace/capability_service.h"
#include "vanta/workspace/workspace_context.h"
#include "vanta/workspace/workspace_runtime.h"

namespace vanta {
namespace {

Value AgentPayload(Value value) {
  return value;
}

void EmitEvent(const AgentOperationRequest &request,
               AgentOperationStatus status, std::string message,
               AgentOperationJournal *journal, AgentOperationCallback &on_event,
               std::optional<Value> payload = std::nullopt) {
  AgentOperationEvent event{
      .operation_id = request.id,
      .kind = request.kind,
      .status = status,
      .message = std::move(message),
      .payload = std::move(payload),
  };
  if (journal != nullptr) {
    journal->RecordEvent(event);
  }
  if (on_event) {
    on_event(event);
  }
}

Value DiagnosticProjection(const Diagnostic &diagnostic) {
  return Value::ObjectValue({
      {"file", Value(diagnostic.location.file.ToUri().ToString())},
      {"line", Value(static_cast<std::int64_t>(diagnostic.location.line))},
      {"column", Value(static_cast<std::int64_t>(diagnostic.location.column))},
      {"severity", Value(ToString(diagnostic.severity))},
      {"source", Value(diagnostic.source)},
      {"message", Value(diagnostic.message)},
  });
}

Value IndexHitProjection(const IndexHit &hit) {
  return Value::ObjectValue({
      {"file", Value(hit.file.ToUri().ToString())},
      {"startLine", Value(static_cast<std::int64_t>(hit.range.start.line))},
      {"startCharacter", Value(static_cast<std::int64_t>(hit.range.start.character))},
      {"endLine", Value(static_cast<std::int64_t>(hit.range.end.line))},
      {"endCharacter", Value(static_cast<std::int64_t>(hit.range.end.character))},
      {"title", Value(hit.title)},
      {"preview", Value(hit.preview)},
      {"providerId", Value(hit.provider_id)},
      {"score", Value(static_cast<std::int64_t>(hit.score))},
  });
}

Value SearchHitsProjection(const std::vector<IndexHit> &hits) {
  Value::Array values;
  for (const IndexHit &hit : hits) {
    values.push_back(IndexHitProjection(hit));
  }
  return Value::ArrayValue(std::move(values));
}

Value BuildResultProjection(const BuildResult &result) {
  Value::Array diagnostics;
  for (const Diagnostic &diagnostic : result.diagnostics) {
    diagnostics.push_back(DiagnosticProjection(diagnostic));
  }
  return Value::ObjectValue({
      {"exitCode", Value(static_cast<std::int64_t>(result.exit_code))},
      {"output", Value(result.output)},
      {"diagnostics", Value::ArrayValue(std::move(diagnostics))},
      {"events", internal::ExecutionEventsProjection(result.events)},
  });
}

AgentOperationResult FailedResult(AgentOperationKind kind, std::string error) {
  return {
      .ok = false,
      .kind = kind,
      .error = std::move(error),
  };
}

std::string RequestSummary(const AgentOperationRequest &request) {
  switch (request.kind) {
  case AgentOperationKind::ReadFile:
  case AgentOperationKind::ProposeFileReplacement:
    return request.file.ToUri().ToString();
  case AgentOperationKind::SearchFiles:
  case AgentOperationKind::SearchText:
    return request.query;
  case AgentOperationKind::ExplainDiagnostic:
    return request.diagnostic.message;
  case AgentOperationKind::RunBuild:
  case AgentOperationKind::RunTest:
    return request.build_request.target_id;
  case AgentOperationKind::CallTool:
    return request.tool_id;
  }
  return {};
}

std::string ResultSummary(const AgentOperationResult &result) {
  if (!result.ok) {
    return result.error;
  }
  if (!result.change_set_id.empty()) {
    return result.change_set_id;
  }
  if (!result.search_hits.empty()) {
    return std::to_string(result.search_hits.size()) + " hits";
  }
  if (!result.text.empty()) {
    return std::to_string(result.text.size()) + " bytes";
  }
  return result.message;
}

Permission PermissionForOperation(AgentOperationKind kind) {
  switch (kind) {
  case AgentOperationKind::ReadFile:
  case AgentOperationKind::SearchFiles:
  case AgentOperationKind::SearchText:
  case AgentOperationKind::ExplainDiagnostic:
    return Permission::WorkspaceRead;
  case AgentOperationKind::ProposeFileReplacement:
    return Permission::WorkspaceWrite;
  case AgentOperationKind::RunBuild:
  case AgentOperationKind::RunTest:
    return Permission::ProcessExecute;
  case AgentOperationKind::CallTool:
    return Permission::WorkspaceRead;
  }
  return Permission::WorkspaceRead;
}

bool HighRiskOperation(AgentOperationKind kind) {
  return kind == AgentOperationKind::ProposeFileReplacement ||
         kind == AgentOperationKind::RunBuild ||
         kind == AgentOperationKind::RunTest;
}

std::string ActionForRequest(const AgentOperationRequest &request) {
  switch (request.kind) {
  case AgentOperationKind::ReadFile:
    return "read " + request.file.ToUri().ToString();
  case AgentOperationKind::SearchFiles:
    return "search files for " + request.query;
  case AgentOperationKind::SearchText:
    return "search text for " + request.query;
  case AgentOperationKind::ExplainDiagnostic:
    return "explain diagnostic";
  case AgentOperationKind::ProposeFileReplacement:
    return "propose replacement for " + request.file.ToUri().ToString();
  case AgentOperationKind::RunBuild:
    return "run build";
  case AgentOperationKind::RunTest:
    return "run test";
  case AgentOperationKind::CallTool:
    return "call agent tool " + request.tool_id;
  }
  return ToString(request.kind);
}

} // namespace

void AgentOperationJournal::RecordStart(const AgentOperationRequest &request) {
  AgentOperationRecord &value = Ensure(request.id, request.kind);
  value.status = AgentOperationStatus::Started;
  value.input_summary = RequestSummary(request);
  value.output_summary.clear();
  value.error.clear();
  value.change_set_id.clear();
  value.ok = false;
  value.events.clear();
}

void AgentOperationJournal::RecordEvent(const AgentOperationEvent &event) {
  AgentOperationRecord &value = Ensure(event.operation_id, event.kind);
  value.status = event.status;
  value.events.push_back(event);
}

void AgentOperationJournal::RecordResult(const std::string &operation_id,
                                         const AgentOperationResult &result) {
  AgentOperationRecord &value = Ensure(operation_id, result.kind);
  value.status = result.ok ? AgentOperationStatus::Completed
                           : AgentOperationStatus::Failed;
  value.output_summary = ResultSummary(result);
  value.error = result.error;
  value.change_set_id = result.change_set_id;
  value.ok = result.ok;
}

std::optional<AgentOperationRecord>
AgentOperationJournal::Record(const std::string &operation_id) const {
  auto it = records_.find(operation_id);
  return it == records_.end() ? std::nullopt
                              : std::optional<AgentOperationRecord>(it->second);
}

std::vector<AgentOperationRecord> AgentOperationJournal::Records() const {
  std::vector<AgentOperationRecord> values;
  for (const std::string &id : order_) {
    auto it = records_.find(id);
    if (it != records_.end()) {
      values.push_back(it->second);
    }
  }
  return values;
}

void AgentOperationJournal::Clear() {
  records_.clear();
  order_.clear();
}

AgentOperationRecord &
AgentOperationJournal::Ensure(const std::string &operation_id,
                              AgentOperationKind kind) {
  AgentOperationRecord &value = records_[operation_id];
  if (value.id.empty()) {
    value.id = operation_id;
    value.kind = kind;
    order_.push_back(operation_id);
  }
  return value;
}

void AgentOperationService::SetJournal(AgentOperationJournal *journal) {
  journal_ = journal;
}

AgentOperationJournal *AgentOperationService::Journal() const {
  return journal_;
}

AgentOperationResult
AgentOperationService::Execute(WorkspaceContext &context,
                               const AgentOperationRequest &request,
                               AgentOperationCallback on_event) const {
  AgentOperationRequest effective = request;
  if (effective.id.empty()) {
    effective.id = NextOperationId();
  }
  if (journal_ != nullptr) {
    journal_->RecordStart(effective);
  }
  auto result_promise = std::make_shared<std::promise<AgentOperationResult>>();
  std::future<AgentOperationResult> result_future = result_promise->get_future();
  JobHandle handle = context.Jobs().Submit(
      context.Async(),
      {
          .kind = JobKind::Agent,
          .title = "Agent operation: " + ToString(effective.kind),
      },
      [this, &context, effective, on_event = std::move(on_event),
       result_promise](JobContext &job) mutable {
        (void)job;
        auto finish = [&](AgentOperationResult value) {
          if (journal_ != nullptr) {
            journal_->RecordResult(effective.id, value);
          }
          context.Capabilities().Set({
              .id = "agent.operations",
              .title = "Agent Operations",
              .provider_id = "vanta.core",
              .status = CapabilityStatus::Available,
              .message = "Agent operation protocol is available",
              .details = {{"records", std::to_string(context.AgentOperationJournal().Records().size())}},
          });
          const AgentOperationResult result = std::move(value);
          result_promise->set_value(result);
          return JobResult{
              .success = result.ok,
              .message = result.ok ? result.message
                                   : (result.error.empty() ? result.message
                                                           : result.error),
              .payload = internal::AgentOperationResultProjection(result),
          };
        };

        try {
          EmitEvent(effective, AgentOperationStatus::Started,
                    ToString(effective.kind), journal_, on_event);
          if (HighRiskOperation(effective.kind) && context.Approvals().RequestApproval({
                  .subject = "agent.operation",
                  .permission = PermissionForOperation(effective.kind),
                  .action = ActionForRequest(effective),
                  .high_risk = HighRiskOperation(effective.kind),
              }) == ApprovalDecision::Deny) {
            const std::string error = "Agent operation was denied";
            EmitEvent(effective, AgentOperationStatus::Failed, error, journal_,
                      on_event);
            return finish(FailedResult(effective.kind, error));
          }

          switch (effective.kind) {
          case AgentOperationKind::ReadFile: {
            auto text = effective.file.ReadText();
            if (!text) {
              const std::string error = "Unable to read file";
              EmitEvent(effective, AgentOperationStatus::Failed, error,
                        journal_, on_event);
              return finish(FailedResult(effective.kind, error));
            }
            EmitEvent(effective, AgentOperationStatus::Completed, "Read file",
                      journal_, on_event);
            return finish({
                .ok = true,
                .kind = effective.kind,
                .message = "Read file",
                .text = std::move(*text),
            });
          }
          case AgentOperationKind::SearchFiles: {
            IndexQueryResult query = context.Indexes().Query(context, {
                .kind = IndexQueryKind::Files,
                .query = effective.query,
                .limit = effective.limit,
            });
            if (!query.ok) {
              const std::string error = query.error.empty() ? "Unable to search files" : query.error;
              EmitEvent(effective, AgentOperationStatus::Failed, error,
                        journal_, on_event);
              return finish(FailedResult(effective.kind, error));
            }
            EmitEvent(effective, AgentOperationStatus::Completed,
                      "Searched files", journal_, on_event,
                      AgentPayload(SearchHitsProjection(query.hits)));
            return finish({
                .ok = true,
                .kind = effective.kind,
                .message = "Searched files",
                .search_hits = std::move(query.hits),
            });
          }
          case AgentOperationKind::SearchText: {
            IndexQueryResult query = context.Indexes().Query(context, {
                .kind = IndexQueryKind::Text,
                .query = effective.query,
                .limit = effective.limit,
            });
            if (!query.ok) {
              const std::string error = query.error.empty() ? "Unable to search text" : query.error;
              EmitEvent(effective, AgentOperationStatus::Failed, error,
                        journal_, on_event);
              return finish(FailedResult(effective.kind, error));
            }
            EmitEvent(effective, AgentOperationStatus::Completed,
                      "Searched text", journal_, on_event,
                      AgentPayload(SearchHitsProjection(query.hits)));
            return finish({
                .ok = true,
                .kind = effective.kind,
                .message = "Searched text",
                .search_hits = std::move(query.hits),
            });
          }
          case AgentOperationKind::ExplainDiagnostic: {
            std::string explanation =
                context.AgentTools().ExplainDiagnostic(effective.diagnostic);
            EmitEvent(effective, AgentOperationStatus::Completed,
                      "Explained diagnostic", journal_, on_event,
                      AgentPayload(DiagnosticProjection(effective.diagnostic)));
            return finish({
                .ok = true,
                .kind = effective.kind,
                .message = "Explained diagnostic",
                .text = std::move(explanation),
            });
          }
          case AgentOperationKind::ProposeFileReplacement: {
            auto original_text =
                effective.file.ReadText();
            if (!original_text) {
              const std::string error =
                  "Unable to read file before creating change set";
              EmitEvent(effective, AgentOperationStatus::Failed, error,
                        journal_, on_event);
              return finish(FailedResult(effective.kind, error));
            }
            const ChangeSet change_set = context.Changes().CreateFileReplacement(
                effective.file, effective.source,
                effective.title.empty() ? "Agent change" : effective.title,
                std::move(*original_text), effective.replacement_text,
                effective.expected_document_version);
            context.Events().Publish({
                .kind = IdeEventKind::ChangeSetProposed,
                .file = effective.file,
                .message = change_set.title,
            });
            EmitEvent(effective, AgentOperationStatus::Completed,
                      "Created change set", journal_, on_event,
                      AgentPayload(Value::ObjectValue({{"changeSetId", Value(change_set.id)}})));
            return finish({
                .ok = true,
                .kind = effective.kind,
                .message = "Created change set",
                .change_set_id = change_set.id,
                .payload = AgentPayload(Value::ObjectValue({{"diff", Value(change_set.unified_diff)}})),
            });
          }
          case AgentOperationKind::RunBuild:
          case AgentOperationKind::RunTest: {
            BuildRequest request = effective.build_request;
            request.kind = effective.kind == AgentOperationKind::RunBuild
                            ? BuildRequestKind::Build
                            : BuildRequestKind::Test;
            BuildResult result = context.Build().Run(
                context, request,
                [&](const ExecutionEvent &event) {
                  EmitEvent(effective, AgentOperationStatus::Progress,
                            event.text, journal_, on_event, AgentPayload(internal::ExecutionEventProjection(event)));
                });
            const bool ok = result.exit_code == 0;
            EmitEvent(effective,
                      ok ? AgentOperationStatus::Completed
                         : AgentOperationStatus::Failed,
                      ok ? "Build operation completed"
                         : "Build operation failed",
                      journal_, on_event, AgentPayload(BuildResultProjection(result)));
            return finish({
                .ok = ok,
                .kind = effective.kind,
                .error = ok ? "" : result.output,
                .message =
                    ok ? "Build operation completed" : "Build operation failed",
                .build_result = std::move(result),
            });
          }
          case AgentOperationKind::CallTool: {
            std::optional<Value> output =
                context.AgentTools().CallTool(effective.tool_id, effective.input);
            if (!output) {
              const std::string error =
                  "Agent tool is not registered: " + effective.tool_id;
              EmitEvent(effective, AgentOperationStatus::Failed, error,
                        journal_, on_event);
              return finish(FailedResult(effective.kind, error));
            }
            Value output_payload = *output;
            EmitEvent(effective, AgentOperationStatus::Completed,
                      "Called agent tool", journal_, on_event, output_payload);
            return finish({
                .ok = true,
                .kind = effective.kind,
                .message = "Called agent tool",
                .payload = output_payload,
            });
          }
          }

          const std::string error = "Unsupported agent operation";
          EmitEvent(effective, AgentOperationStatus::Failed, error, journal_,
                    on_event);
          return finish(FailedResult(effective.kind, error));
        } catch (const std::exception &error) {
          EmitEvent(effective, AgentOperationStatus::Failed, error.what(),
                    journal_, on_event);
          return finish(FailedResult(effective.kind, error.what()));
        } catch (...) {
          const std::string error = "Agent operation failed";
          EmitEvent(effective, AgentOperationStatus::Failed, error, journal_,
                    on_event);
          return finish(FailedResult(effective.kind, error));
        }
      });

  AgentOperationResult result = result_future.get();
  handle.Wait();
  return result;
}

std::string AgentOperationService::NextOperationId() const {
  return "operation-" + std::to_string(next_operation_id_++);
}

std::string ToString(AgentOperationKind kind) {
  switch (kind) {
  case AgentOperationKind::ReadFile:
    return "readFile";
  case AgentOperationKind::SearchFiles:
    return "SearchFiles";
  case AgentOperationKind::SearchText:
    return "SearchText";
  case AgentOperationKind::ExplainDiagnostic:
    return "explainDiagnostic";
  case AgentOperationKind::ProposeFileReplacement:
    return "proposeFileReplacement";
  case AgentOperationKind::RunBuild:
    return "runBuild";
  case AgentOperationKind::RunTest:
    return "runTest";
  case AgentOperationKind::CallTool:
    return "callTool";
  }
  return "readFile";
}

std::string ToString(AgentOperationStatus status) {
  switch (status) {
  case AgentOperationStatus::Started:
    return "started";
  case AgentOperationStatus::Progress:
    return "progress";
  case AgentOperationStatus::Completed:
    return "completed";
  case AgentOperationStatus::Failed:
    return "failed";
  }
  return "started";
}

} // namespace vanta
