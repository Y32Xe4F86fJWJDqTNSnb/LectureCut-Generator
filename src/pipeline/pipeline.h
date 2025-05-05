#pragma once

#include <deque>
#include <unordered_map>

#include <optional>

#include <functional>
#include <algorithm>
#include <ranges>

#include <mutex>
#include <shared_mutex>
#include <condition_variable>
#include <semaphore>

#include <chrono>

#include <cassert>

#include <format>

template <std::movable T, std::movable M>
class PipelineQueue
{
public:
  using value_type = T;
  using metadata_type = M;

public:
  PipelineQueue(
    std::size_t capacity = 36, 
    std::chrono::milliseconds timeout = std::chrono::milliseconds(5'000)
  )
    : m_nextIndexToPush(0)
    , m_nextIndexToPop(0)
    , m_capacity(capacity)
    , m_timeout(timeout)
    , m_metadata(timeout)
  {
  }

  void push(
    std::convertible_to<T> auto && data,
    std::optional<std::size_t> optGlobalElementIndex = std::nullopt
  )
  {
    {
      auto const itProducerDone = m_workersDone.find(std::this_thread::get_id());
      if(itProducerDone == m_workersDone.end())
        throw std::runtime_error("A producer was not registered as active before pushing to the queue.");
      else if(itProducerDone->second)
        throw std::runtime_error("A producer had already been declared finished before pushing to the queue.");
    }

    bool const suppliedElementIndex = optGlobalElementIndex.has_value();

    {
      std::unique_lock<std::shared_mutex> lockQueue(m_mtxQueueAccess);

      std::size_t index {};

      if(suppliedElementIndex) {
        index = optGlobalElementIndex.value();
      }

      auto const withinRange =
        [this, &index, suppliedElementIndex] ()
        {
          if(!suppliedElementIndex)
          {
            index = m_nextIndexToPush;
          }

          if(suppliedElementIndex && index < m_nextIndexToPop)
          {
            throw std::runtime_error(std::format(
              "Cannot push item with index {:d} because it is not greater than the oldest item in the queue with index {:d}",
              index, m_nextIndexToPop
            ));
          }

          return (index >= m_nextIndexToPop) && (index - m_nextIndexToPop < m_capacity);
        };

      if(!m_cvQueueAccess.wait_for(lockQueue, m_timeout, withinRange))
        throw std::runtime_error("Queue starvation on side of producer");

      m_queue.emplace_back(std::make_pair(index, std::forward<decltype(data)>(data)));
      assert(std::ranges::count(m_queue, &std::optional<std::pair<std::size_t, T>>::has_value) <= m_capacity);

      ++m_nextIndexToPush;
    }

    m_cvQueueAccess.notify_all();
  }

  std::optional<std::size_t> pop(T & data)
  {
    std::optional<std::size_t> optIndex = std::nullopt;

    {
      std::unique_lock<std::shared_mutex> lockQueue(m_mtxQueueAccess); 

      typename decltype(m_queue)::iterator itOldestItem;
      bool isNonEmpty {};

      auto const isNextIndexToPopOrEmpty =
        [this, &itOldestItem, &isNonEmpty] ()
        {
          itOldestItem = p_findOldestItem();

          isNonEmpty = itOldestItem != m_queue.end() && itOldestItem->has_value();

          return (!isNonEmpty && p_allProducersDone()) || (isNonEmpty && itOldestItem->value().first == m_nextIndexToPop);
        };

      if(!m_cvQueueAccess.wait_for(lockQueue, m_timeout, isNextIndexToPopOrEmpty))
        throw std::runtime_error("Queue starvation on side of consumer");

      if(isNonEmpty)
      {
        std::size_t index {};
        std::tie(index, data) = std::move(itOldestItem->value());
        itOldestItem->reset();
        assert(index == m_nextIndexToPop);
        ++m_nextIndexToPop;

        optIndex.emplace(index);

        m_queue.erase(
          std::ranges::begin(m_queue),
          std::ranges::find_if(m_queue, &std::optional<std::pair<std::size_t, T>>::has_value)
        );

        lockQueue.unlock();
        m_cvQueueAccess.notify_all();
      }
    }

    return optIndex;
  }

  void registerProducerActive()
  {
    std::scoped_lock<std::shared_mutex> lockQueue(m_mtxQueueAccess);

    bool const itemWasPresent = !m_workersDone.emplace(std::this_thread::get_id(), false).second;

    if(itemWasPresent)
      throw std::runtime_error("A producer cannot register as active more than once.");
  }

  void registerProducerDone()
  {
    std::shared_lock<std::shared_mutex> lockQueue(m_mtxQueueAccess);

    auto const itProducerDone = m_workersDone.find(std::this_thread::get_id());

    if(itProducerDone == m_workersDone.end())
      throw std::runtime_error("A producer had not been registered as active before being declared done.");

    auto & producerDone = itProducerDone->second;

    if(producerDone)
      throw std::runtime_error("A producer cannot register as done more than once.");

    producerDone = true;
    m_cvQueueAccess.notify_all();
  }

  std::size_t size() const
  {
    std::scoped_lock<std::shared_mutex> lockQueue(m_mtxQueueAccess);

    return m_queue.size();
  }

  void setMetadata(std::convertible_to<M> auto && metadata)
  {
    m_metadata.setMetadata(std::forward<decltype(metadata)>(metadata));
  }

  void getMetadata(M & metadata) const
  {
    m_metadata.getMetadata(metadata);
  }

private:
  auto p_findOldestItem(this auto && self)
  {
    return
      std::ranges::min_element(
        self.m_queue,
        [](const auto & lhs, const auto & rhs)
        {
          return
            lhs.has_value() &&
            (
              !rhs.has_value()
              || (rhs.has_value() && lhs->first < rhs->first)
            );
        }
      );
  }

  auto p_allProducersDone() const
  {
    return
      std::all_of(
        m_workersDone.begin(), m_workersDone.end(),
        std::mem_fn(&decltype(m_workersDone)::value_type::second)
      );
  }

private:
  template <typename M>
  class Metadata
  {
  public:
    using metadata_type = M;

  public:
    Metadata(std::chrono::milliseconds timeout = std::chrono::milliseconds(5'000))
      : m_smphValueIsSet(0)
      , m_timeout(timeout)
    {
    }

    void setMetadata(std::convertible_to<M> auto && metadata)
    {
      {
        std::scoped_lock<std::shared_mutex> lock(m_mtxValueAccess);

        if(m_metadata.has_value())
          throw std::runtime_error("Metadata has been set more than once.");

        m_metadata = std::make_optional(std::forward<decltype(metadata)>(metadata));
      }
      m_smphValueIsSet.release(m_smphValueIsSet.max());
    }

    void getMetadata(M & metadata) const
    {
      if(!m_smphValueIsSet.try_acquire_for(m_timeout))
        throw std::runtime_error("Metadata starvation");
      {
        std::shared_lock<std::shared_mutex> lock(m_mtxValueAccess);

        metadata = m_metadata.value();
      }
      m_smphValueIsSet.release();
    }

  private:
    std::optional<M> m_metadata;
    std::chrono::milliseconds m_timeout;
    mutable std::counting_semaphore<std::numeric_limits<std::ptrdiff_t>::max()> m_smphValueIsSet;
    mutable std::shared_mutex m_mtxValueAccess;
  };

private:
  Metadata<M> m_metadata;
  std::deque<std::optional<std::pair<std::size_t, T>>> m_queue;
  std::size_t m_nextIndexToPush, m_nextIndexToPop, m_capacity;
  std::unordered_map<std::thread::id, bool> m_workersDone;
  std::chrono::milliseconds m_timeout;
  mutable std::condition_variable_any m_cvQueueAccess;
  mutable std::shared_mutex m_mtxQueueAccess;
};
