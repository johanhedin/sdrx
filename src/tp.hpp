//
// Thread pool for sdrx
//
// @author Johan Hedin
// @date   2026
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

#ifndef TP_HPP
#define TP_HPP

#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <queue>
#include <vector>

class ThreadPool {
public:
    explicit ThreadPool(unsigned thread_count) : stop_(false) {
        for (unsigned i = 0; i < thread_count; ++i) {
            threads_.emplace_back(&ThreadPool::worker_, this);
        }
    }

    ~ThreadPool() {
        {
            std::unique_lock<std::mutex> lock(mutex_);
            stop_ = true;
        }
        condition_.notify_all();
        for (auto &t : threads_) {
            t.join();
        }
    }

    void submit(std::function<void()> task) {
        {
            std::unique_lock<std::mutex> lock(mutex_);
            queue_.push(std::move(task));
        }
        condition_.notify_one();
    }

private:
    bool                              stop_;
    std::mutex                        mutex_;
    std::condition_variable           condition_;
    std::queue<std::function<void()>> queue_;
    std::vector<std::thread>          threads_;

    void worker_() {
        while (true) {
            std::function<void()> task;
            {
                std::unique_lock<std::mutex> lock(mutex_);
                condition_.wait(lock, [this]{ return !queue_.empty() || stop_; });
                if (stop_ && queue_.empty()) break;
                task = std::move(queue_.front());
                queue_.pop();
            }
            task();
        }
    }
};

#endif // TP_HPP
