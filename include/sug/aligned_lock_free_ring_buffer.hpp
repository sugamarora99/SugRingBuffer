#ifndef ALIGNED_SPSC
#define ALIGNED_SPSC

#include <atomic>
#include <memory>
#include <new>
#include <stdexcept>
#include "spsc_interface.hpp"

namespace SugRingBuffer {

    /*
        Lock-free ring buffer implementation that has cache aligned read/write pointers.
        This guarantees that those two pointers will NOT be on the same cache line,
        thus preventing cache pingpong "false-sharing".

        This implementation also avoids % operation, since it compiles to integer division
        operation, which is one of the most expensive ALU operations.
        Bit masking is much faster.

    */
    template<typename T>
    class AlignedSPSC : public SPSCInterface<T> {
        public:
            AlignedSPSC() = delete;

            //not copyable or movable
            AlignedSPSC(const AlignedSPSC& other) = delete;
            AlignedSPSC(AlignedSPSC&& other) = delete;
            AlignedSPSC& operator=(const AlignedSPSC& other) = delete;
            AlignedSPSC& operator=(AlignedSPSC&& other) = delete;
            
            explicit AlignedSPSC(size_t capacity) : capacity_{capacity} {
                // capacity must be a power of 2
                // buffer will hold (capacity-1 items)
                if(capacity < 2 || (capacity & (capacity-1)) != 0){
                    throw std::logic_error("Capacity must be a power of 2!");
                }
                buffer = std::make_unique<T[]>(capacity);
            }

            bool push(const T& item) override{
                if(full()){
                    return false;
                }
                int currentIdx = writeIdx_.load(std::memory_order_relaxed);
                buffer[currentIdx] = item;
                writeIdx_.store((currentIdx+1) & (capacity_ - 1), std::memory_order_release);
                size_.fetch_add(1, std::memory_order_relaxed);
                return true;
            }

            bool pop(T& item) override{
                if(empty()){
                    return false;
                }
                int currentIdx = readIdx_.load(std::memory_order_relaxed);
                item = buffer[currentIdx];
                readIdx_.store((currentIdx+1) & (capacity_ - 1), std::memory_order_release);
                size_.fetch_sub(1, std::memory_order_relaxed);
                return true;
            }

            [[nodiscard]] bool full() const override {
                // producer thread will be only thread that calls this
                if(((writeIdx_.load(std::memory_order_relaxed) + 1) & (capacity_ - 1)) == readIdx_.load(std::memory_order_acquire)){
                    return true;
                } else{
                    return false;
                }
                
            }

            [[nodiscard]] bool empty() const override{
                // consumer thread will be only thread that calls this
                if(readIdx_.load(std::memory_order_relaxed) == writeIdx_.load(std::memory_order_acquire)){
                    return true;
                } else{
                    return false;
                }
            }

            [[nodiscard]] size_t size() const override {
                return size_.load(std::memory_order_relaxed);
            }

        private:
            size_t capacity_;
            std::unique_ptr<T[]> buffer;
            std::atomic<size_t> size_{0};
            
            alignas(std::hardware_destructive_interference_size)
            std::atomic<size_t> readIdx_{0};

            alignas(std::hardware_destructive_interference_size)
            std::atomic<size_t> writeIdx_{0};

    };
};   //namespace SugRingBuffer


#endif