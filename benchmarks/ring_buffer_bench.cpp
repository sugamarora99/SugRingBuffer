#ifdef __linux__
#define _GNU_SOURCE
#endif

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <format>
#include <iostream>
#include <string>
#include <string_view>
#include <thread>

#ifdef __linux__
#include <pthread.h>
#include <sched.h>
#endif

#include "sug/aligned_lock_free_ring_buffer.hpp"
#include "sug/lock_free_ring_buffer.hpp"

namespace {

// False sharing only manifests when producer and consumer run on DIFFERENT
// cores (cross-core cache invalidation traffic). Pin to guarantee this.
void pin_this_thread(std::string_view name, int cpu) {
#ifdef __linux__
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(cpu, &set);

    const int rc = pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &set);
    if (rc != 0) {
        std::cerr << "[" << name << "] pthread_setaffinity_np(cpu=" << cpu
                  << ") failed: " << std::strerror(rc) << "\n";
    }
#else
    (void)name;
    (void)cpu;  // macOS: no reliable core-pinning API
#endif
}

}  // namespace

// <aligned/nonaligned> <# of items to push> <capacity> <pin 0|1>
int main(int argc, char* argv[]) {
    if (argc != 5) {
        std::cout << "invalid input there are " << argc
                  << " args, args are <aligned/nonaligned> <# of items to push> <capacity> <pin 0|1>\n";
        return 1;
    }

    const std::string_view mode(argv[1]);
    const bool aligned = (mode == "aligned");
    const std::size_t numItems = std::stoull(argv[2]);
    const std::size_t capacity = std::stoull(argv[3]);
    const bool pin = (std::stoull(argv[4]) != 0);

    SugRingBuffer::SPSCInterface<std::uint64_t>* ringBuf;
    if (aligned) {
        ringBuf = new SugRingBuffer::AlignedSPSC<std::uint64_t>(capacity);
    } else {
        ringBuf = new SugRingBuffer::SPSC<std::uint64_t>(capacity);
    }

    std::cout << "launching producer and consumer threads\n";
    const auto start = std::chrono::steady_clock::now();

    std::thread producer([&]() {
        if (pin) pin_this_thread("producer", 0);  // producer -> CPU 0
        for (std::uint64_t i = 0; i < numItems; ++i) {
            while (!ringBuf->push(i)) {}  // spin while full
        }
    });

    std::thread consumer([&]() {
        if (pin) pin_this_thread("consumer", 1);  // consumer -> CPU 1
        for (std::uint64_t i = 0; i < numItems; ++i) {
            std::uint64_t item = 0;
            while (!ringBuf->pop(item)) {}  // spin while empty
        }
    });

    producer.join();
    consumer.join();

    const auto end = std::chrono::steady_clock::now();
    const double seconds = std::chrono::duration<double>(end - start).count();
    const double throughput = numItems / seconds / 1'000'000.0;

    std::cout << std::format(
        "mode={} capacity={} num_items={} pin={} time={:.3f}s throughput={:.3f}M items/s\n",
        mode, capacity, numItems, (pin ? 1 : 0), seconds, throughput);

    delete ringBuf;
}
