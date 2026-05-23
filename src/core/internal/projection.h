#pragma once

#include <string>
#include <vector>

#include "mornox/agent/agent_context.h"
#include "mornox/agent/agent_operation.h"
#include "mornox/core/value.h"
#include "mornox/execution/execution_service.h"
#include "mornox/execution/run_configuration.h"
#include "mornox/language/code_intelligence_service.h"
#include "mornox/language/language_service.h"

namespace mornox::internal {

Value AgentContextProjection(const AgentContext& context);
Value AgentOperationEventProjection(const AgentOperationEvent& event);
Value AgentOperationResultProjection(const AgentOperationResult& result);
Value DiagnosticProjection(const Diagnostic& diagnostic);
Value DiagnosticsProjection(const std::vector<Diagnostic>& diagnostics);
Value IndexHitProjection(const IndexHit& hit);
Value IndexHitsProjection(const std::vector<IndexHit>& hits);
Value BuildResultProjection(const BuildResult& result);

Value ExecutionTargetProjection(const ExecutionTarget& target);
Value ExecutionEventProjection(const ExecutionEvent& event);
Value ExecutionEventsProjection(const std::vector<ExecutionEvent>& events);
Value RunConfigurationProjection(const RunConfiguration& configuration);
Value RunResultProjection(const RunResult& result);

Value LanguageCompletionProjection(const CompletionList& result);
Value LanguageHoverProjection(const HoverResult& result);
Value LanguageLocationProjection(const LocationResult& result);
Value LanguageSemanticTokensProjection(const SemanticTokens& result);
Value LanguageErrorProjection(const std::string& error);
Value CodeIntelligenceResultProjection(const CodeIntelligenceResult& result);

}
