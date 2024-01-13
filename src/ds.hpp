//
// Downsampler in sdrx
//
// @author Johan Hedin
// @date   2024
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

#ifndef DS_HPP
#define DS_HPP

#include <thread>
#include <mutex>
#include <condition_variable>
#include <latch>

#include "msd.hpp"

class DS {
public:
    DS(const MSD &msd) : run_(true), in_data_ptr_(nullptr), msd_(msd), thread_(&DS::worker_, this) {}
    ~DS(void) {
        std::unique_lock<std::mutex> lock(mutex_);
        run_ = false;
        lock.unlock();
        condition_.notify_one();
        thread_.join();
    }

    void addJob(const iqsample_t *data, unsigned data_len, iqsample_t *out, std::latch &latch) {
        std::unique_lock<std::mutex> lock(mutex_);

        in_data_ptr_ = data;
        in_data_len_ = data_len;
        out_ptr_ = out;
        latch_ = &latch;

        lock.unlock();
        condition_.notify_one();
    }

private:
    // Order matters. Variables used in the thread must be before thread_
    bool                    run_;
    const iqsample_t       *in_data_ptr_;
    unsigned                in_data_len_;
    iqsample_t             *out_ptr_;
    MSD                     msd_;
    std::mutex              mutex_;
    std::condition_variable condition_;
    std::thread             thread_;
    std::latch             *latch_;

    void worker_(void) {
        std::unique_lock<std::mutex> lock(mutex_);
        while (run_) {
            if (in_data_ptr_) {
                msd_.decimate(in_data_ptr_, in_data_len_, out_ptr_);
                in_data_ptr_ = nullptr;
                latch_->count_down();
                latch_ = nullptr;
            }

            condition_.wait(lock); // mutex is unlocked during wait
        }
    }
};

#endif // DS_HPP
