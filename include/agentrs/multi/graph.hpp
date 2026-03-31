#pragma once
// graph.hpp — Directed agent graph for graph-based routing.
// Equivalent to agentrs-multi/src/graph.rs

#include <functional>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "agentrs/core/types.hpp"

namespace agentrs {

// Forward declarations
class AgentGraphBuilder;

// --- EdgeCondition ----------------------------------------------------------

/// Transition predicate for graph orchestration.
struct EdgeCondition {
    enum class Type {
        /// Always traverse this edge.
        Always,
        /// Traverse when output contains a keyword.
        Contains,
        /// Custom predicate function.
        Custom,
        /// Terminal edge marker — never traversed.
        End
    };

    Type type = Type::Always;

    /// Keyword for Contains condition.
    std::string keyword;

    /// Custom predicate for Custom condition.
    std::function<bool(const AgentOutput&)> predicate;

    /// Creates an Always condition.
    static EdgeCondition always() {
        EdgeCondition c;
        c.type = Type::Always;
        return c;
    }

    /// Creates a Contains condition.
    static EdgeCondition contains(const std::string& kw) {
        EdgeCondition c;
        c.type = Type::Contains;
        c.keyword = kw;
        return c;
    }

    /// Creates a Custom condition with a user-defined predicate.
    static EdgeCondition custom(std::function<bool(const AgentOutput&)> pred) {
        EdgeCondition c;
        c.type = Type::Custom;
        c.predicate = std::move(pred);
        return c;
    }

    /// Creates an End (terminal) condition.
    static EdgeCondition end() {
        EdgeCondition c;
        c.type = Type::End;
        return c;
    }

    /// Evaluates the condition against an agent output.
    [[nodiscard]] bool evaluate(const AgentOutput& output) const;
};

// --- GraphEdge --------------------------------------------------------------

/// Graph edge between two agent nodes.
struct GraphEdge {
    /// Source node name.
    std::string from;
    /// Target node name.
    std::string to;
    /// Edge predicate.
    EdgeCondition condition;
};

// --- AgentGraph -------------------------------------------------------------

/// Directed agent graph: nodes are agent names, edges define conditional flow.
class AgentGraph {
public:
    AgentGraph() = default;

    /// Starts building a graph.
    static AgentGraphBuilder builder();

    /// Returns the entry point node name, or throws InvalidConfigurationError.
    [[nodiscard]] const std::string& entry() const;

    /// Determines the next node given the current node and the output.
    /// Returns std::nullopt if no matching edge is found (terminal state).
    [[nodiscard]] std::optional<std::string> next(const std::string& current,
                                                   const AgentOutput& output) const;

    /// Returns all registered node names.
    [[nodiscard]] const std::map<std::string, std::string>& nodes() const { return nodes_; }

    /// Returns all edges.
    [[nodiscard]] const std::vector<GraphEdge>& edges() const { return edges_; }

private:
    friend class AgentGraphBuilder;

    std::map<std::string, std::string> nodes_;
    std::vector<GraphEdge> edges_;
    std::optional<std::string> entry_;
};

// --- AgentGraphBuilder ------------------------------------------------------

/// Builder for AgentGraph.
class AgentGraphBuilder {
public:
    AgentGraphBuilder() = default;

    /// Adds a node by name.
    AgentGraphBuilder& node(const std::string& name);

    /// Adds an edge with a condition.
    AgentGraphBuilder& edge(const std::string& from, const std::string& to,
                            EdgeCondition condition);

    /// Sets the graph entry node.
    AgentGraphBuilder& entry(const std::string& entry_name);

    /// Finalizes the graph. Throws InvalidConfigurationError on invalid config.
    [[nodiscard]] AgentGraph build() const;

private:
    AgentGraph graph_;
};

} // namespace agentrs
