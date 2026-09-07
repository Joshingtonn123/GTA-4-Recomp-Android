#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <utility>
#include <vector>

namespace rex::graphics::gta4_native {

template <typename Entry>
class NativeOwnerRetirementWatch;

// Owner destruction may run on any thread; GPU destruction runs only in the
// consumer's DrainCompleted callback. Entries must have stable addresses and
// remain alive until that callback, or until Close precedes bulk cache teardown.
// Close and DrainCompleted are called only by the same consumer thread.
template <typename Entry>
class NativeOwnerRetirementQueue {
 public:
  NativeOwnerRetirementQueue() = default;
  ~NativeOwnerRetirementQueue() { Close(); }
  NativeOwnerRetirementQueue(const NativeOwnerRetirementQueue&) = delete;
  NativeOwnerRetirementQueue& operator=(const NativeOwnerRetirementQueue&) = delete;

  template <typename Submission, typename Release>
  void DrainCompleted(uint64_t completed_submission, Submission last_submission, Release release) {
    std::unique_ptr<Batch> expired;
    {
      std::lock_guard lock(mutex_);
      if (closed_) {
        return;
      }
      expired = std::move(expired_);
    }
    while (expired) {
      auto batch = std::move(expired);
      expired = std::move(batch->next);
      for (Entry* entry : batch->entries) {
        // Once its CPU owner is gone, this entry cannot gain another GPU use.
        const uint64_t submission = last_submission(*entry);
        if (submission <= completed_submission) {
          release(*entry);
        } else {
          waiting_[submission].push_back(entry);
        }
      }
    }
    while (!waiting_.empty() && waiting_.begin()->first <= completed_submission) {
      auto completed = waiting_.begin();
      for (Entry* entry : completed->second) {
        release(*entry);
      }
      waiting_.erase(completed);
    }
  }

  // Closure never dereferences entries. A producer that already locked its
  // weak queue can finish publishing after Close without touching erased maps.
  void Close() {
    std::unique_ptr<Batch> abandoned;
    {
      std::lock_guard lock(mutex_);
      closed_ = true;
      abandoned = std::move(expired_);
    }
    // Iterative disposal also bounds the stack when many owners expire at once.
    while (abandoned) {
      auto batch = std::move(abandoned);
      abandoned = std::move(batch->next);
    }
    waiting_.clear();
  }

 private:
  friend class NativeOwnerRetirementWatch<Entry>;
  struct Batch {
    std::vector<Entry*> entries;
    std::unique_ptr<Batch> next;
  };

  void Publish(std::unique_ptr<Batch> batch) {
    std::lock_guard lock(mutex_);
    if (!closed_) {
      batch->next = std::move(expired_);
      expired_ = std::move(batch);
    }
  }

  std::mutex mutex_;
  bool closed_ = false;
  std::unique_ptr<Batch> expired_;
  std::map<uint64_t, std::vector<Entry*>> waiting_;
};

// Embedded in the immutable CPU snapshot, initialized only by the render worker
// while it holds a strong snapshot reference. One batch allocation per owner;
// destruction publishes that existing batch without allocating or running GPU
// callbacks. Register each persistent entry once, when inserting it in its map.
template <typename Entry>
class NativeOwnerRetirementWatch {
 public:
  NativeOwnerRetirementWatch() = default;
  ~NativeOwnerRetirementWatch() { Publish(); }
  NativeOwnerRetirementWatch(const NativeOwnerRetirementWatch&) = delete;
  NativeOwnerRetirementWatch& operator=(const NativeOwnerRetirementWatch&) = delete;

  void Track(const std::shared_ptr<NativeOwnerRetirementQueue<Entry>>& queue, Entry& entry) {
    if (!batch_ || queue_.lock() != queue) {
      Publish();
      queue_ = queue;
      batch_ = std::make_unique<typename NativeOwnerRetirementQueue<Entry>::Batch>();
    }
    batch_->entries.push_back(&entry);
  }

 private:
  void Publish() {
    if (!batch_) {
      return;
    }
    if (const auto queue = queue_.lock()) {
      queue->Publish(std::move(batch_));
    } else {
      batch_.reset();
    }
  }

  std::weak_ptr<NativeOwnerRetirementQueue<Entry>> queue_;
  std::unique_ptr<typename NativeOwnerRetirementQueue<Entry>::Batch> batch_;
};

}  // namespace rex::graphics::gta4_native
