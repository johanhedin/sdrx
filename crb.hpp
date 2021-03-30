//
// Lock-free thread safe Single Producer, Single Consumer chunked ring buffer
//
// @author Johan Hedin
// @date   2021-03-29
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


#ifndef CRB_HPP
#define CRB_HPP

#include <memory>
#include <atomic>
#include <vector>

// Lock-free thread safe Single Producer, Single Consumer chunked ring buffer
// with continuous write and read and chunk metadata block.
//
// Based on the following information, ideas and implementations:
//
//     https://www.codeproject.com/Articles/3479/The-Bip-Buffer-The-Circular-Buffer-with-a-Twist
//     https://ferrous-systems.com/blog/lock-free-ring-buffer
//     https://www.codeproject.com/articles/43510/lock-free-single-producer-single-consumer-circular
//     https://github.com/KjellKod/lock-free-wait-free-circularfifo/blob/master/src/circularfifo_memory_relaxed_aquire_release_padded.hpp
//     https://en.cppreference.com/w/cpp/thread/hardware_destructive_interference_size
//
// The buffer has two states, state 1 and state 2. The buffer is in state 1
// when the write pointer leads the read pointer and in state 2 when the read
// pointer leads the write pointer.
//
// Transistion from state 1 to state 2 can only happen from the writer thread.
// Transistion from state 2 to state 1 can only happen from the reader thread.

template<typename T, typename M>
class CRB {
public:
    CRB(size_t chunk_size, size_t num_chunks) : chunks_(num_chunks+1),
    write_ptr_(0), read_ptr_(0), end_ptr_(num_chunks), capacity_(num_chunks+1),
    acquired_write_len_(0), acquired_read_len_(0) {
        for (auto &c : chunks_) {
            c.buf_ = std::make_unique<T[]>(chunk_size + ALING_LEN * 2);
        }
    }

    ~CRB(void) {}

    CRB(void) = delete;
    CRB(const CRB&) = delete;
    CRB& operator=(const CRB&) = delete;

    bool acquireWrite(T **buf,  M **m) {
        const size_t rd_ptr = read_ptr_.load(std::memory_order_acquire);
        const size_t wr_ptr = write_ptr_.load(std::memory_order_relaxed);

        acquired_write_len_ = 0;

        // Check what state the buffer is in
        if (wr_ptr >= rd_ptr) {
            // State 1 (write leads read)
            //
            // We can write up to, but not including, capacity_
            if (wr_ptr + 1 < capacity_) {
                // The requested amount of items to write will fit in the remainig
                // of the buffer
                acquired_write_ptr_ = wr_ptr;
                acquired_write_len_ = 1;
                acquired_end_ptr_   = capacity_ - 1;
            } else {
                // The requested amount of items to write will not fit in the
                // remainig of the buffer. Check if we can wrap around and find
                // space in the beginning of the buffer. If not, the buffer is full
                if (1 < rd_ptr) {
                    // Ok to write the data at the beginning of the buffer
                    acquired_write_ptr_ = 0;
                    acquired_write_len_ = 1;
                    acquired_end_ptr_   = wr_ptr;
                }
            }
        } else {
            // State 2 (read leads write)
            //
            // We can write upto, but not including, read_ptr_
            if (wr_ptr + 1 < rd_ptr) {
                acquired_write_ptr_ = wr_ptr;
                acquired_write_len_ = 1;

                // In state 2 we do not touch the end pointer
            }
        }

        // No space in the buffer or zero items requested
        if (acquired_write_len_ == 0) return false;

        *buf = &chunks_[acquired_write_ptr_].buf_[64];
        *m   = &chunks_[acquired_write_ptr_].m_;

        return true;
    }

    bool commitWrite(void) {
        // Not allowed to call this function unless a call to acquireWrite
        // indicated that space is actually available.
        if (acquired_write_len_ == 0) return false;

        // The write to end_prt_ will be syncronized by the store operation on
        // write_ptr_ below
        end_ptr_ = acquired_end_ptr_;

        acquired_write_len_ = 0;

        // If acquired_write_ptr_ was set to 0 in acquireWrite, this store
        // operation will change the buffer state from 1 to 2
        write_ptr_.store(acquired_write_ptr_ + 1, std::memory_order_release);

        return true;
    }

    bool acquireRead(T const **buf, M const **m) {
        const size_t wr_ptr = write_ptr_.load(std::memory_order_acquire);
        const size_t rd_ptr = read_ptr_.load(std::memory_order_relaxed);

        // Calculate how many item that are available in the buffer.
        // The calculation is different depending on the buffer state.
        if (wr_ptr >= rd_ptr) {
            // State 1 (write leads read).
            //
            // We can read up to, but not including or beyond, write_ptr_.
            // If the buffer is empty, i.e. wr_ptr == rd_ptr, acquired_read_len_
            // will be zero and the function will return false.

            acquired_read_ptr_ = rd_ptr;
            acquired_read_len_ = wr_ptr - rd_ptr;
        } else {
            // State 2 (read leads write).
            //
            // We can read up to, but not including or beond, end_ptr_

            if (rd_ptr < end_ptr_) {
                acquired_read_ptr_ = rd_ptr;
                acquired_read_len_ = end_ptr_ - rd_ptr;
            } else {
                // Wrap around
                acquired_read_ptr_ = 0;
                acquired_read_len_ = wr_ptr;
            }
        }

        // Return false if the buffer is empty
        if (acquired_read_len_ == 0) return false;

        *buf = &chunks_[acquired_read_ptr_].buf_[64];
        *m   = &chunks_[acquired_read_ptr_].m_;

        return true;
    }

    bool commitRead(void) {
        // Not allowed to call this function unless a call to acquireRead
        // indicated that data is actually available.
        if (acquired_read_len_ == 0) return false;

        acquired_read_len_ = 0;

        // Atomically commit the update for the read pointer. If
        // acquired_read_ptr_ was set to 0 in acquireRead (wrap around), this
        // store operation will change the buffer state from 2 to 1.
        read_ptr_.store(acquired_read_ptr_ + 1, std::memory_order_release);

        return true;
    }

private:
    static const size_t ALING_LEN = 64; // Typical cache line size
    class Chunk {
    public:
        unsigned char         align_buf1_[ALING_LEN];
        std::unique_ptr<T[]>  buf_;
        M                     m_;
        unsigned char         align_buf2_[ALING_LEN];
    };

    // Variables used by both threads
    std::vector<Chunk>         chunks_;
    unsigned char              align_buf1_[ALING_LEN];
    std::atomic<size_t>        write_ptr_;  // Write pointer
    unsigned char              align_buf2_[ALING_LEN];
    std::atomic<size_t>        read_ptr_;   // Read pointer
    unsigned char              align_buf3_[ALING_LEN];
    size_t                     end_ptr_;    // Current end of buffer. Used for wrap around. Not needed to be atomic since write_ptr_ will fence
    unsigned char              align_buf4_[ALING_LEN];

    // Variables used only by the writing thread
    const size_t capacity_;            // Capacity. Const and can not be changed. +1 from requested to accommodate for sentinel
    size_t       acquired_write_ptr_;  // Acquired write pointer
    size_t       acquired_write_len_;  // Acquired write len
    size_t       acquired_end_ptr_;    // Acquired end pointer

    // Variables used only by the reading thread
    size_t acquired_read_ptr_;  // Acquired read pointer
    size_t acquired_read_len_;  // Acquired read len
};

#endif // CRB_HPP
