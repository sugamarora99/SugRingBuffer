#ifndef LOCK_FREE_RING_BUFFER_HPP
#define LOCK_FREE_RING_BUFFER_HPP

#include <atomic>
#include <memory>
#include <stdexcept>
#include "spsc_interface.hpp"

namespace SugRingBuffer {

    template<typename T>
    class SPSC : public SPSCInterface<T> {
        public:
            SPSC() = delete;

            //not copyable or movable
            SPSC(const SPSC& other) = delete;
            SPSC(SPSC&& other) = delete;
            SPSC& operator=(const SPSC& other) = delete;
            SPSC& operator=(SPSC&& other) = delete;

            explicit SPSC(size_t capacity) : capacity_{capacity} {
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
                return true;
            }

            bool pop(T& item) override{
                if(empty()){
                    return false;
                }
                int currentIdx = readIdx_.load(std::memory_order_relaxed);
                item = buffer[currentIdx];
                readIdx_.store((currentIdx+1) & (capacity_ - 1), std::memory_order_release);
                return true;
            }

            [[nodiscard]] bool full() const override{
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

            [[nodiscard]] size_t size() const override{
                const size_t w = writeIdx_.load(std::memory_order_relaxed);
                const size_t r = readIdx_.load(std::memory_order_relaxed);

                // unsigned int wrap around is 2^64, so we can use bitmask since capacity_ is a power of 2
                return (w - r) & (capacity_ - 1);
            }

        private:
            size_t capacity_;
            std::unique_ptr<T[]> buffer;
            std::atomic<size_t> readIdx_{0};
            std::atomic<size_t> writeIdx_{0};
    };

};  //namespace SugRingBuffer

#endif
