
#include <chrono>
#include <iostream>
#include <thread>
#include <string>
#include <format>
#include <string_view>
#include "sug/lock_free_ring_buffer.hpp"
#include "sug/aligned_lock_free_ring_buffer.hpp"

// <aligned/nonaligned> <# of items to push> <capacity>
int main(int argc, char* argv[]){
    //TODO: false sharing here will only happen when producer and consumer
    // are running on DIFFERENT cores (cross-core cache invalidation traffic), 
    // so use cpu pinning to guarantee this
    
    if(argc != 4){
        std::cout << "invalid input there are " << argc << " args, args are <aligned/nonaligned> <# of items to push> <capacity>\n";
        return 1;
    }
    std::string_view mode(argv[1]);
    bool aligned = (mode == "aligned") ? true : false;
    size_t numItems = std::stoull(argv[2]);
    size_t capacity = std::stoull(argv[3]);

    SugRingBuffer::SPSCInterface<uint64_t>* ringBuff;
    if(aligned){
        ringBuff = new SugRingBuffer::AlignedSPSC<uint64_t>(capacity);
    } else{
        ringBuff = new SugRingBuffer::SPSC<uint64_t>(capacity);
    }

    std::cout << "lauching producer and consumer threads\n";
    auto start = std::chrono::steady_clock::now();
    std::thread producer([&](){
        for(uint64_t i = 0; i < numItems; i++){
            while(!ringBuff->push(i)){} // spin while buffer is full
        }
    });

    std::thread consumer([&](){
        for(uint64_t i = 0; i < numItems; i++){
            uint64_t item;
            while(!ringBuff->pop(item)){}   // spin while buffer is empty
        }
    });

    producer.join();
    consumer.join();
    auto end = std::chrono::steady_clock::now();
    double seconds = std::chrono::duration<double>(end-start).count();
    double throughput = numItems/seconds/1'000'000;
    std::cout << std::format("mode={} capacity={} num_items={} time={:.3f}s throughput={:.3f}M items/s\n", mode, capacity, numItems, seconds, throughput);
}