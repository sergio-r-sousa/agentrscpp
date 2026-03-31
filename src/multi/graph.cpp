// graph.cpp — Directed agent graph implementation.

#include "agentrs/multi/graph.hpp"

#include "agentrs/core/error.hpp"

namespace agentrs {

// --- EdgeCondition ----------------------------------------------------------

bool EdgeCondition::evaluate(const AgentOutput& output) const {
    switch (type) {
        case Type::Always:
            return true;
        case Type::Contains:
            return output.text.find(keyword) != std::string::npos;
        case Type::Custom:
            return predicate ? predicate(output) : false;
        case Type::End:
            return false;
    }
    return false;
}

// --- AgentGraph -------------------------------------------------------------

AgentGraphBuilder AgentGraph::builder() {
    return AgentGraphBuilder{};
}

const std::string& AgentGraph::entry() const {
    if (!entry_.has_value()) {
        throw InvalidConfigurationError("graph entry point not set");
    }
    return entry_.value();
}

std::optional<std::string> AgentGraph::next(const std::string& current,
                                             const AgentOutput& output) const {
    for (const auto& edge : edges_) {
        if (edge.from == current && edge.condition.evaluate(output)) {
            return edge.to;
        }
    }
    return std::nullopt;
}

// --- AgentGraphBuilder ------------------------------------------------------

AgentGraphBuilder& AgentGraphBuilder::node(const std::string& name) {
    graph_.nodes_[name] = name;
    return *this;
}

AgentGraphBuilder& AgentGraphBuilder::edge(const std::string& from,
                                            const std::string& to,
                                            EdgeCondition condition) {
    graph_.edges_.push_back(GraphEdge{from, to, std::move(condition)});
    return *this;
}

AgentGraphBuilder& AgentGraphBuilder::entry(const std::string& entry_name) {
    graph_.entry_ = entry_name;
    return *this;
}

AgentGraph AgentGraphBuilder::build() const {
    // Validate entry is set.
    const auto& entry_name = graph_.entry();

    // Validate entry node is registered.
    if (graph_.nodes_.find(entry_name) == graph_.nodes_.end()) {
        throw InvalidConfigurationError(
            "graph entry node '" + entry_name + "' is not registered");
    }
    return graph_;
}

} // namespace agentrs
