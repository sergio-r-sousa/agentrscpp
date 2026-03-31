#pragma once
// vector.hpp — Vector memory with simple keyword matching.
// Equivalent to agentrs-memory/src/vector.rs
//
// The Rust version uses real embeddings + cosine similarity.
// This C++ version uses a simple deterministic embedder (byte-bucket hashing)
// and cosine similarity for retrieval — matching the Rust SimpleEmbedder.

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

#include "agentrs/core/interfaces.hpp"
#include "agentrs/core/types.hpp"
#include "agentrs/memory/in_memory.hpp"

namespace agentrs {

/// Search result returned by a vector store.
struct VectorSearchResult {
    float score = 0.0f;
    Message payload;
};

/// Computes embeddings for messages.
/// Equivalent to Rust's `Embedder` async trait.
class IEmbedder {
public:
    virtual ~IEmbedder() = default;

    /// Generates an embedding vector.
    [[nodiscard]] virtual std::vector<float> embed(std::string_view text) const = 0;
};

/// Persists and searches embedding vectors.
/// Equivalent to Rust's `VectorStore` async trait.
class IVectorStore {
public:
    virtual ~IVectorStore() = default;

    /// Upserts a vector and payload.
    virtual void upsert(const std::string& id, std::vector<float> vec, const Message& payload) = 0;

    /// Searches the store.
    [[nodiscard]] virtual std::vector<VectorSearchResult> search(const std::vector<float>& query,
                                                                  std::size_t limit) const = 0;

    /// Clears all stored vectors.
    virtual void clear() = 0;
};

/// Small deterministic embedder useful for tests and local demos.
/// Maps text bytes into 16 buckets — equivalent to Rust's `SimpleEmbedder`.
class SimpleEmbedder : public IEmbedder {
public:
    [[nodiscard]] std::vector<float> embed(std::string_view text) const override;
};

/// In-memory vector store with cosine similarity search.
/// Equivalent to Rust's `InMemoryVectorStore`.
class InMemoryVectorStore : public IVectorStore {
public:
    InMemoryVectorStore() = default;

    void upsert(const std::string& id, std::vector<float> vec, const Message& payload) override;

    [[nodiscard]] std::vector<VectorSearchResult> search(const std::vector<float>& query,
                                                          std::size_t limit) const override;

    void clear() override;

private:
    // Each item: (id, embedding, payload)
    mutable std::mutex mutex_;
    std::vector<std::tuple<std::string, std::vector<float>, Message>> items_;
};

/// Memory backend that combines recent history with semantic retrieval.
/// Equivalent to Rust's `VectorMemory`.
class VectorMemory : public IMemory {
public:
    /// Creates a vector memory with built-in SimpleEmbedder and InMemoryVectorStore.
    VectorMemory();

    /// Creates a vector memory with custom embedder and store.
    VectorMemory(std::shared_ptr<IEmbedder> embedder, std::shared_ptr<IVectorStore> store);

    ~VectorMemory() override = default;

    // Movable
    VectorMemory(VectorMemory&&) noexcept = default;
    VectorMemory& operator=(VectorMemory&&) noexcept = default;

    // Copyable (shared_ptr members)
    VectorMemory(const VectorMemory&) = default;
    VectorMemory& operator=(const VectorMemory&) = default;

    // --- IMemory ------------------------------------------------------------

    void store(std::string_view key, const Message& value) override;

    [[nodiscard]] std::vector<Message> retrieve(std::string_view query,
                                                 std::size_t limit) override;

    [[nodiscard]] std::vector<Message> history() const override;

    void clear() override;

    // --- Extra (mirrors Rust SearchableMemory) ------------------------------

    /// Returns the approximate token count for the recent history.
    [[nodiscard]] std::size_t token_count() const;

private:
    std::shared_ptr<IEmbedder> embedder_;
    std::shared_ptr<IVectorStore> store_;
    InMemoryMemory recent_;
};

// --- Free functions ---------------------------------------------------------

/// Cosine similarity between two vectors.
float cosine_similarity(const std::vector<float>& left, const std::vector<float>& right);

} // namespace agentrs
