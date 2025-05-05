#pragma once

#include <deque>
#include <algorithm>
#include <ranges>
#include <condition_variable>
#include <iostream>

#define VERSION "0.1.1"

#define PROGRESS_BAR_NAME "Generating"

constexpr static int PCM_SAMPLE_RATE = 48000;

class PCM_QUEUE 
{
public:
  void push(std::int16_t const * data, std::size_t size) 
  {
    std::unique_lock<std::mutex> lock(mutex);
    std::copy_n(data, size, std::back_inserter(queue));
    lock.unlock();
    cv.notify_one();
  }

  std::size_t pop(std::int16_t * data, std::size_t size) 
  {
    std::unique_lock<std::mutex> lock(mutex);
    cv.wait(lock, [&] { return done || queue.size() >= size; });
    
    auto const actualSize = std::min(size, queue.size());

    std::copy_n(queue.begin(), actualSize, data);

    queue.erase(
      queue.begin(), 
      std::next(queue.begin(), actualSize)
    );
    
    lock.unlock();
    cv.notify_one();
    return actualSize;
  }

  std::size_t size() 
  {
    std::lock_guard<std::mutex> lock(mutex);
    return queue.size();
  }

  void set_done() 
  {
    std::unique_lock<std::mutex> lock(mutex);
    done = true;
    lock.unlock();
    cv.notify_one();
  }

  bool is_done() 
  {
    return done;
  }

private:
  std::deque<std::int16_t> queue;
  std::mutex mutex;
  std::condition_variable cv;
  bool done = false;
};