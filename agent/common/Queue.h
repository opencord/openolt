//
// Copyright (c) 2013 Juan Palacios juan.palacios.puyana@gmail.com
// Subject to the BSD 2-Clause License
// - see < http://opensource.org/licenses/BSD-2-Clause>
//

#ifndef CONCURRENT_QUEUE_
#define CONCURRENT_QUEUE_

#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>

template <typename T>
class Queue
{
 public:

  // timeout is in milliseconds, wait_granularity in milliseconds
  std::pair<T, bool> pop(int timeout, int wait_granularity=10)
  {
    std::cv_status status = std::cv_status::no_timeout;
    std::unique_lock<std::mutex> mlock(mutex_);
    static int duration = 0;
    if (timeout < wait_granularity) {
        wait_granularity = timeout;
    }
    while (queue_.empty())
    {
      status = cond_.wait_for(mlock, std::chrono::milliseconds(wait_granularity));
      if (status == std::cv_status::timeout)
      {
        duration+=wait_granularity;
        if (duration > timeout)
        {
          duration = 0;
          return std::pair<T, bool>({}, false);
        }
      }
    }
    auto val = queue_.front();
    queue_.pop();
    return std::pair<T, bool>(val, true);
  }

  void pop(T& item)
  {
    std::unique_lock<std::mutex> mlock(mutex_);
    while (queue_.empty())
    {
      cond_.wait(mlock);
    }
    item = queue_.front();
    queue_.pop();
  }

  void push(const T& item)
  {
    std::unique_lock<std::mutex> mlock(mutex_);
    queue_.push(item);
    mlock.unlock();
    cond_.notify_one();
  }
  Queue()=default;
  Queue(const Queue&) = delete;            // disable copying
  Queue& operator=(const Queue&) = delete; // disable assignment
  
 private:
  std::queue<T> queue_;
  std::mutex mutex_;
  std::condition_variable cond_;
};

#endif
