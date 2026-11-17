#ifndef LOCK_FREE_RING_BUFFER_HPP
#define LOCK_FREE_RING_BUFFER_HPP

#include <optional>
#include <atomic>
#include <vector>
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
            
            explicit SPSC(size_t capacity){
                capacity_ = capacity;
                std::vector<std::optional<T>> temp(capacity_, std::nullopt);
                buffer = std::move(temp);
            }

            bool push(const T& item) override{
                if(full()){
                    return false;
                }
                int currentIdx = writeIdx_.load(std::memory_order_relaxed);
                buffer[currentIdx] = item;
                writeIdx_.store((currentIdx+1) % capacity_, std::memory_order_release);
                return true;
            }

            bool pop(T& item) override{
                if(empty()){
                    return false;
                }
                int currentIdx = readIdx_.load(std::memory_order_relaxed);
                item = buffer[currentIdx].value();
                readIdx_.store((currentIdx+1) % capacity_, std::memory_order_release);
                return true;
            }

            [[nodiscard]] bool full() const override{
                // producer thread will be only thread that calls this
                if((writeIdx_.load(std::memory_order_relaxed) + 1) % capacity_ == readIdx_.load(std::memory_order_acquire)){
                    return true;
                } else{
                    return false;
                }
                
            }

            [[nodiscard]] bool empty() const override {
                // consumer thread will be only thread that calls this
                if(readIdx_.load(std::memory_order_relaxed) == writeIdx_.load(std::memory_order_acquire)){
                    return true;
                } else{
                    return false;
                }
            }

            [[nodiscard]] size_t size() const override {
                // no thread ever relies on size(), so putting memory barriers around access don't have any benefit.
                const size_t w = writeIdx_.load(std::memory_order_relaxed);
                const size_t r = readIdx_.load(std::memory_order_relaxed);
                return (w + capacity_ - r) % capacity_;
            }

        private:
            std::vector<std::optional<T>> buffer;
            size_t capacity_;
            std::atomic<size_t> writeIdx_{0};
            std::atomic<size_t> readIdx_{0};
    };


};  //namespace SugRingBuffer




#endif