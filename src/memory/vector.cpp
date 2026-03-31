// vector.cpp — VectorMemory implementation.
// Equivalent to agentrs-memory/src/vector.rs

#include "agentrs/memory/vector.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <string>

#include "agentrs/core/types.hpp"

namespace agentrs {

// ---------------------------------------------------------------------------
// cosine_similarity (free function)
// ---------------------------------------------------------------------------

float cosine_similarity(const std::vector<float>& left,
                        const std::vector<float>& right) {
    if (left.size() != right.size() || left.empty()) {
        return 0.0f;
    }

    float dot = 0.0f;
    float left_norm = 0.0f;
    float right_norm = 0.0f;

    for (std::size_t i = 0; i < left.size(); ++i) {
        dot += left[i] * right[i];
        left_norm += left[i] * left[i];
        right_norm += right[i] * right[i];
    }

    left_norm = std::sqrt(left_norm);
    right_norm = std::sqrt(right_norm);

    if (left_norm == 0.0f || right_norm == 0.0f) {
        return 0.0f;
    }
    return dot / (left_norm * right_norm);
}

// ---------------------------------------------------------------------------
// SimpleEmbedder
// ---------------------------------------------------------------------------

std::vector<float> SimpleEmbedder::embed(std::string_view text) const {
    // Deterministic 16-bucket hashing — mirrors the Rust SimpleEmbedder.
    std::vector<float> buckets(16, 0.0f);
    std::size_t index = 0;
    for (unsigned char byte : text) {
        buckets[index % 16] += static_cast<float>(byte) / 255.0f;
        ++index;
    }
    return buckets;
}

// ---------------------------------------------------------------------------
// InMemoryVectorStore
// ---------------------------------------------------------------------------

void InMemoryVectorStore::upsert(const std::string& id,
                                  std::vector<float> vec,
                                  const Message& payload) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& item : items_) {
        if (std::get<0>(item) == id) {
            item = std::make_tuple(id, std::move(vec), payload);
            return;
        }
    }
    items_.emplace_back(id, std::move(vec), payload);
}

std::vector<VectorSearchResult> InMemoryVectorStore::search(
    const std::vector<float>& query,
    std::size_t limit) const {

    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<VectorSearchResult> scored;
    scored.reserve(items_.size());

    for (const auto& [id, vec, payload] : items_) {
        VectorSearchResult r;
        r.score = cosine_similarity(vec, query);
        r.payload = payload;
        scored.push_back(std::move(r));
    }

    // Sort by score descending.
    std::sort(scored.begin(), scored.end(),
              [](const VectorSearchResult& a, const VectorSearchResult& b) {
                  return a.score > b.score;
              });

    if (scored.size() > limit) {
        scored.resize(limit);
    }
    return scored;
}

void InMemoryVectorStore::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    items_.clear();
}

// ---------------------------------------------------------------------------
// VectorMemory — construction
// ---------------------------------------------------------------------------

VectorMemory::VectorMemory()
    : embedder_(std::make_shared<SimpleEmbedder>()),
      store_(std::make_shared<InMemoryVectorStore>()),
      recent_() {}

VectorMemory::VectorMemory(std::shared_ptr<IEmbedder> embedder,
                             std::shared_ptr<IVectorStore> store)
    : embedder_(std::move(embedder)),
      store_(std::move(store)),
      recent_() {}

// ---------------------------------------------------------------------------
// IMemory
// ---------------------------------------------------------------------------

void VectorMemory::store(std::string_view key, const Message& value) {
    // Embed and upsert into the vector store.
    auto vec = embedder_->embed(value.text_content());
    std::string id = std::string(key) + "-" + generate_uuid();
    store_->upsert(id, std::move(vec), value);

    // Also keep in the recent in-memory history.
    recent_.store(key, value);
}

std::vector<Message> VectorMemory::retrieve(std::string_view query,
                                             std::size_t limit) {
    auto vec = embedder_->embed(query);
    auto results = store_->search(vec, limit);

    std::vector<Message> messages;
    messages.reserve(results.size());
    for (auto& r : results) {
        messages.push_back(std::move(r.payload));
    }
    return messages;
}

std::vector<Message> VectorMemory::history() const {
    return recent_.history();
}

void VectorMemory::clear() {
    store_->clear();
    recent_.clear();
}

// ---------------------------------------------------------------------------
// SearchableMemory equivalent
// ---------------------------------------------------------------------------

std::size_t VectorMemory::token_count() const {
    std::size_t total = 0;
    for (const auto& msg : recent_.history()) {
        total += msg.text_content().length() / 4;
    }
    return total;
}

} // namespace agentrs
