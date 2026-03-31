#pragma once
// agentrs.hpp — Facade header for the agentrs C++ SDK.
// Equivalent to `use agentrs::prelude::*` in Rust.
//
// Include this single header to get all public types, interfaces,
// providers, tools, memory backends, multi-agent orchestration, and
// YAML configuration support.

// ── Core types, error hierarchy, interfaces, streaming, testing ─────────────

#include "agentrs/core/types.hpp"
#include "agentrs/core/error.hpp"
#include "agentrs/core/interfaces.hpp"
#include "agentrs/core/streaming.hpp"
#include "agentrs/core/testing.hpp"

// ── Result<T,E> and TRY macro ──────────────────────────────────────────────

#include "agentrs/result.hpp"

// ── Tools: registry + built-in implementations ─────────────────────────────

#include "agentrs/tools/registry.hpp"
#include "agentrs/tools/builtin.hpp"

// ── MCP: protocol, client, adapter ─────────────────────────────────────────

#include "agentrs/mcp/protocol.hpp"
#include "agentrs/mcp/client.hpp"
#include "agentrs/mcp/adapter.hpp"

// ── Memory backends ────────────────────────────────────────────────────────

#include "agentrs/memory/in_memory.hpp"
#include "agentrs/memory/sliding_window.hpp"
#include "agentrs/memory/token_aware.hpp"
#include "agentrs/memory/vector.hpp"
#include "agentrs/memory/redis_backend.hpp"

// ── LLM providers + registry ───────────────────────────────────────────────

#include "agentrs/llm/openai.hpp"
#include "agentrs/llm/azureopenai.hpp"
#include "agentrs/llm/anthropic.hpp"
#include "agentrs/llm/gemini.hpp"
#include "agentrs/llm/ollama.hpp"
#include "agentrs/llm/registry.hpp"

// ── Agent builder + runner ─────────────────────────────────────────────────

#include "agentrs/agents/builder.hpp"
#include "agentrs/agents/runner.hpp"

// ── Multi-agent: communication, graph, shared memory, orchestrator ─────────

#include "agentrs/multi/communication.hpp"
#include "agentrs/multi/graph.hpp"
#include "agentrs/multi/shared_memory.hpp"
#include "agentrs/multi/orchestrator.hpp"

// ── YAML config loader ─────────────────────────────────────────────────────

#include "agentrs/config.hpp"

// ── .env file loader ───────────────────────────────────────────────────────

#include "agentrs/dotenv.hpp"

// ── Convenience using declarations (mirrors Rust prelude re-exports) ───────

namespace agentrs {

// Re-export nothing extra — all symbols are already in namespace agentrs
// from the headers above. Users write:
//
//   #include <agentrs/agentrs.hpp>
//   using namespace agentrs;
//
// Which is the C++ equivalent of Rust's `use agentrs::prelude::*`.

} // namespace agentrs
