#ifndef LOCK_FREE_RING_BUFFER_HPP
#define LOCK_FREE_RING_BUFFER_HPP

#include <optional>
#include <atomic>
#include <vector>

namespace SugRingBuffer {

    template<typename T>
    class SPSC {
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

            bool push(const T& item){
                if(full()){
                    return false;
                }
                int currentIdx = writeIdx_.load(std::memory_order_relaxed);
                buffer[currentIdx] = item;
                writeIdx_.store((currentIdx+1) % capacity_, std::memory_order_release);
                size_.fetch_add(1, std::memory_order_relaxed);
                return true;
            }

            bool pop(T& item){
                if(empty()){
                    return false;
                }
                int currentIdx = readIdx_.load(std::memory_order_relaxed);
                item = buffer[currentIdx].value();
                readIdx_.store((currentIdx+1) % capacity_, std::memory_order_release);
                size_.fetch_sub(1, std::memory_order_relaxed);
                return true;
            }

            [[nodiscard]] bool full() const {
                // producer thread will be only thread that calls this
                if((writeIdx_.load(std::memory_order_relaxed) + 1) % capacity_ == readIdx_.load(std::memory_order_acquire)){
                    return true;
                } else{
                    return false;
                }
                
            }

            [[nodiscard]] bool empty() const {
                // consumer thread will be only thread that calls this
                if(readIdx_.load(std::memory_order_relaxed) == writeIdx_.load(std::memory_order_acquire)){
                    return true;
                } else{
                    return false;
                }
            }

            [[nodiscard]] size_t size() const {
                return size_.load(std::memory_order_relaxed);
            }

        private:
            std::vector<std::optional<T>> buffer;
            size_t capacity_;
            std::atomic<size_t> size_{0};   // no thread ever relies on size(), so putting memory barriers around access don't have any benefit.
            std::atomic<int> writeIdx_{0};
            std::atomic<int> readIdx_{0};
    };


};  //namespace SugRingBuffer




#endif