//
// Test program for CRB — lock-free SPSC chunked ring buffer
//
// Exercises:
//   - Empty/full boundary conditions
//   - Data and metadata integrity (single-threaded)
//   - State 1 -> State 2 -> State 1 wrap-around transitions
//   - Many consecutive wrap cycles
//   - Multi-threaded producer/consumer stress test
//
// Written with help of Claude Code.

#include <iostream>
#include <iomanip>
#include <thread>
#include <vector>
#include <cstdint>
#include <cstring>
#include <chrono>
#include <pthread.h>
#include <sched.h>

#include "crb.hpp"

// ---------------------------------------------------------------------------
// Test configuration
// ---------------------------------------------------------------------------
static constexpr size_t   CHUNK_SIZE   = 4096;        // T elements per chunk
static constexpr size_t   NUM_CHUNKS   = 4;           // ring buffer capacity
static constexpr uint32_t STRESS_ITERS = 2'000'000u;  // chunks in stress test

struct Meta {
    uint32_t seq;   // sequence number
    uint32_t len;   // payload length in bytes
};

using TestCRB = CRB<uint8_t, Meta>;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// Deterministic data pattern keyed on sequence number
static void fill_pattern(uint8_t *buf, size_t len, uint32_t seq) {
    for (size_t i = 0; i < len; i++) {
        buf[i] = static_cast<uint8_t>((seq + i) & 0xFF);
    }
}

static bool check_pattern(const uint8_t *buf, size_t len, uint32_t seq) {
    for (size_t i = 0; i < len; i++) {
        if (buf[i] != static_cast<uint8_t>((seq + i) & 0xFF)) return false;
    }
    return true;
}

static bool report(const char *name, bool ok, const char *reason = nullptr) {
    if (ok) {
        std::cout << "  PASS  " << name << "\n";
    } else {
        std::cout << "  FAIL  " << name;
        if (reason) std::cout << "  [" << reason << "]";
        std::cout << "\n";
    }
    return ok;
}

// ---------------------------------------------------------------------------
// CPU affinity helpers
// ---------------------------------------------------------------------------

// Returns the list of CPU cores this process is allowed to run on.
static std::vector<int> allowed_cores() {
    std::vector<int> cores;
    cpu_set_t cpuset;
    if (pthread_getaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset) != 0)
        return cores;
    for (int i = 0; i < CPU_SETSIZE; i++) {
        if (CPU_ISSET(i, &cpuset))
            cores.push_back(i);
    }
    return cores;
}

// Pins the calling thread to a single CPU core.
// Returns true on success.
static bool pin_to_core(int core) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core, &cpuset);
    return pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset) == 0;
}

// ---------------------------------------------------------------------------
// Single-threaded tests
// ---------------------------------------------------------------------------

// acquireRead on a freshly constructed buffer must return false
bool test_empty_read() {
    TestCRB crb(CHUNK_SIZE, NUM_CHUNKS);
    const uint8_t *buf; const Meta *m;
    bool ok = !crb.acquireRead(&buf, &m);
    return report("empty_read", ok, "acquireRead succeeded on empty buffer");
}

// commitWrite/commitRead without a preceding successful acquire must fail
bool test_spurious_commit() {
    TestCRB crb(CHUNK_SIZE, NUM_CHUNKS);
    bool ok = !crb.commitWrite() && !crb.commitRead();
    return report("spurious_commit", ok, "commit succeeded without prior acquire");
}

// Write one chunk with a known pattern and metadata, read it back and verify
bool test_single_write_read() {
    TestCRB crb(CHUNK_SIZE, NUM_CHUNKS);
    uint8_t *wbuf; Meta *wm;

    if (!crb.acquireWrite(&wbuf, &wm))
        return report("single_write_read", false, "acquireWrite failed on empty buffer");

    fill_pattern(wbuf, CHUNK_SIZE, 42);
    wm->seq = 42;
    wm->len = CHUNK_SIZE;
    crb.commitWrite();

    const uint8_t *rbuf; const Meta *rm;
    if (!crb.acquireRead(&rbuf, &rm))
        return report("single_write_read", false, "acquireRead failed after one write");

    bool ok = (rm->seq == 42) && (rm->len == CHUNK_SIZE) &&
              check_pattern(rbuf, CHUNK_SIZE, 42);
    crb.commitRead();

    // Buffer must be empty again
    if (ok) {
        const uint8_t *tmp; const Meta *tm;
        ok = !crb.acquireRead(&tmp, &tm);
    }

    return report("single_write_read", ok, "data/metadata mismatch or buffer not empty after drain");
}

// Fill to capacity, verify the buffer is then full, then drain and verify empty
bool test_fill_and_drain() {
    TestCRB crb(CHUNK_SIZE, NUM_CHUNKS);
    uint8_t *wbuf; Meta *wm;

    for (size_t i = 0; i < NUM_CHUNKS; i++) {
        if (!crb.acquireWrite(&wbuf, &wm))
            return report("fill_and_drain", false, "acquireWrite failed before capacity reached");
        fill_pattern(wbuf, CHUNK_SIZE, (uint32_t)i);
        wm->seq = (uint32_t)i;
        wm->len = CHUNK_SIZE;
        crb.commitWrite();
    }

    // Next write must fail — buffer is full
    if (crb.acquireWrite(&wbuf, &wm))
        return report("fill_and_drain", false, "acquireWrite succeeded on full buffer");

    // Drain and check FIFO order and data integrity
    const uint8_t *rbuf; const Meta *rm;
    for (size_t i = 0; i < NUM_CHUNKS; i++) {
        if (!crb.acquireRead(&rbuf, &rm))
            return report("fill_and_drain", false, "acquireRead failed while chunks remain");
        if (rm->seq != (uint32_t)i || !check_pattern(rbuf, CHUNK_SIZE, (uint32_t)i)) {
            crb.commitRead();
            return report("fill_and_drain", false, "seq or data mismatch during drain");
        }
        crb.commitRead();
    }

    // Must be empty now
    const uint8_t *tmp; const Meta *tm;
    bool ok = !crb.acquireRead(&tmp, &tm);
    return report("fill_and_drain", ok, "buffer not empty after full drain");
}

// Exercise one complete State 1 -> State 2 -> State 1 wrap-around cycle.
//
// With NUM_CHUNKS=4 (capacity=5, sentinel at slot 4):
//
//   Write seq 0-3  : positions 0,1,2,3  -> wr=4
//   Read  seq 0,1  : rd advances to 2
//   Write seq 4    : wr+1==capacity, 1<rd -> wrap to pos 0, end_ptr=4, wr=1  (State 2)
//   Read  seq 2,3  : rd advances to 4 == end_ptr
//   Read  seq 4    : reader wraps to pos 0, rd=1                              (State 1)
//   Buffer empty   : wr==rd==1
bool test_wrap_around() {
    TestCRB crb(CHUNK_SIZE, NUM_CHUNKS);
    uint8_t *wbuf; Meta *wm;
    const uint8_t *rbuf; const Meta *rm;

    uint32_t wseq = 0;
    uint32_t rseq = 0;

    // Write 4 chunks into positions 0..3
    for (int i = 0; i < 4; i++) {
        if (!crb.acquireWrite(&wbuf, &wm))
            return report("wrap_around", false, "initial fill write failed");
        fill_pattern(wbuf, CHUNK_SIZE, wseq);
        wm->seq = wseq++;
        wm->len = CHUNK_SIZE;
        crb.commitWrite();
    }

    // Read 2 chunks to advance rd_ptr to 2
    for (int i = 0; i < 2; i++) {
        if (!crb.acquireRead(&rbuf, &rm))
            return report("wrap_around", false, "partial drain read failed");
        if (rm->seq != rseq || !check_pattern(rbuf, CHUNK_SIZE, rseq)) {
            crb.commitRead();
            return report("wrap_around", false, "data mismatch in partial drain");
        }
        crb.commitRead();
        rseq++;
    }

    // This write must wrap (State 1 -> State 2)
    if (!crb.acquireWrite(&wbuf, &wm))
        return report("wrap_around", false, "wrap-around write failed");
    fill_pattern(wbuf, CHUNK_SIZE, wseq);
    wm->seq = wseq++;
    wm->len = CHUNK_SIZE;
    crb.commitWrite();

    // Read remaining 3 chunks: 2 from the tail, 1 from the wrapped head
    for (int i = 0; i < 3; i++) {
        if (!crb.acquireRead(&rbuf, &rm))
            return report("wrap_around", false, "post-wrap read failed");
        if (rm->seq != rseq || !check_pattern(rbuf, CHUNK_SIZE, rseq)) {
            crb.commitRead();
            return report("wrap_around", false, "data mismatch after wrap");
        }
        crb.commitRead();
        rseq++;
    }

    // Buffer must be empty and back in State 1
    const uint8_t *tmp; const Meta *tm;
    bool ok = !crb.acquireRead(&tmp, &tm);
    return report("wrap_around", ok, "buffer not empty after wrap drain");
}

// Drive many interleaved write/read operations to exercise repeated wrap cycles
bool test_multiple_wraps() {
    TestCRB crb(CHUNK_SIZE, NUM_CHUNKS);
    uint8_t *wbuf; Meta *wm;
    const uint8_t *rbuf; const Meta *rm;

    static constexpr size_t TOTAL = NUM_CHUNKS * 50;
    uint32_t wseq = 0;
    uint32_t rseq = 0;
    size_t   written = 0;
    size_t   read_n  = 0;

    while (read_n < TOTAL) {
        // Write if space available and we still have chunks to send
        if (written < TOTAL && crb.acquireWrite(&wbuf, &wm)) {
            fill_pattern(wbuf, CHUNK_SIZE, wseq);
            wm->seq = wseq++;
            wm->len = CHUNK_SIZE;
            crb.commitWrite();
            written++;
        }
        // Read if data available
        if (crb.acquireRead(&rbuf, &rm)) {
            if (rm->seq != rseq || !check_pattern(rbuf, CHUNK_SIZE, rseq)) {
                crb.commitRead();
                return report("multiple_wraps", false, "seq or data mismatch");
            }
            crb.commitRead();
            rseq++;
            read_n++;
        }
    }

    return report("multiple_wraps", true);
}

// ---------------------------------------------------------------------------
// Multi-threaded stress test
// ---------------------------------------------------------------------------
//
// Producer thread writes STRESS_ITERS chunks, each stamped with a sequence
// number and a deterministic data pattern.  The consumer (main thread) reads
// every chunk and verifies sequence order and data integrity.
// Reports chunk throughput on completion.

bool test_threaded_stress() {
    // Pin producer and consumer to distinct physical cores so the test
    // exercises real concurrent memory ordering, not time-sliced interleaving
    // on a single core.  We derive the core list from the process affinity
    // mask so the test works correctly under taskset/cgroup/container
    // constraints.
    const std::vector<int> cores = allowed_cores();
    const bool can_pin = cores.size() >= 2;
    if (can_pin) {
        std::cout << "  [pinning consumer to core " << cores[0]
                  << ", producer to core " << cores[1] << "]\n";
    } else {
        std::cout << "  [WARNING: fewer than 2 allowed cores ("
                  << cores.size() << ") — threads may share a core]\n";
    }

    TestCRB crb(CHUNK_SIZE, NUM_CHUNKS);

    bool consumer_ok = true;
    uint32_t first_error_seq = UINT32_MAX;

    // Consumer core: cores[0]  (set before spawning the producer)
    if (can_pin) pin_to_core(cores[0]);

    std::thread producer([&crb, &cores, can_pin]() {
        if (can_pin) pin_to_core(cores[1]);

        uint8_t *buf; Meta *m;
        for (uint32_t seq = 0; seq < STRESS_ITERS; seq++) {
            while (!crb.acquireWrite(&buf, &m))
                ; // busy-spin: keeps producer hot and maximises contention
            fill_pattern(buf, CHUNK_SIZE, seq);
            m->seq = seq;
            m->len = CHUNK_SIZE;
            crb.commitWrite();
        }
    });

    {
        const uint8_t *buf; const Meta *m;
        for (uint32_t expect = 0; expect < STRESS_ITERS; expect++) {
            while (!crb.acquireRead(&buf, &m))
                ; // busy-spin: keeps consumer hot and maximises contention

            if (consumer_ok) {
                if (m->seq != expect || !check_pattern(buf, CHUNK_SIZE, expect)) {
                    consumer_ok = false;
                    first_error_seq = expect;
                }
            }
            crb.commitRead();
        }
    }

    producer.join();

    if (!consumer_ok) {
        char reason[64];
        std::snprintf(reason, sizeof(reason), "integrity error at seq %u", first_error_seq);
        return report("threaded_stress", false, reason);
    }
    return report("threaded_stress", true);
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------
int main() {
    bool all_pass = true;

    std::cout << "CRB single-threaded tests (chunk_size=" << CHUNK_SIZE
              << ", num_chunks=" << NUM_CHUNKS << "):\n";
    all_pass &= test_empty_read();
    all_pass &= test_spurious_commit();
    all_pass &= test_single_write_read();
    all_pass &= test_fill_and_drain();
    all_pass &= test_wrap_around();
    all_pass &= test_multiple_wraps();

    std::cout << "\nCRB multi-threaded stress test (" << STRESS_ITERS << " chunks, "
              << CHUNK_SIZE << " bytes each):\n";
    auto t0 = std::chrono::steady_clock::now();
    all_pass &= test_threaded_stress();
    auto t1 = std::chrono::steady_clock::now();
    double elapsed_s = std::chrono::duration<double>(t1 - t0).count();
    double mchunks_s = STRESS_ITERS / elapsed_s / 1e6;
    double mb_s      = mchunks_s * CHUNK_SIZE;
    std::cout << std::fixed << std::setprecision(2)
              << "  Throughput: " << mchunks_s << " Mchunks/s  ("
              << mb_s << " MB/s)\n";

    std::cout << "\n" << (all_pass ? "ALL TESTS PASSED" : "SOME TESTS FAILED") << "\n";
    return all_pass ? 0 : 1;
}

