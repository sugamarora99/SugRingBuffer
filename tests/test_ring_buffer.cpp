#include <gtest/gtest.h>

#include <atomic>
#include <cstddef>
#include <thread>
#include <type_traits>
#include <vector>

#include "sug/aligned_lock_free_ring_buffer.hpp"
#include "sug/lock_free_ring_buffer.hpp"

namespace {

constexpr std::size_t next_power_of_two(std::size_t n) {
    std::size_t p = 2;
    while (p < n) {
        p <<= 1;
    }
    return p;
}

template <typename Q>
class SPSCQueueTest : public ::testing::Test {};

using QueueTypes = ::testing::Types<SugRingBuffer::SPSC<int>,
                                    SugRingBuffer::AlignedSPSC<int>>;

TYPED_TEST_SUITE(SPSCQueueTest, QueueTypes);

// empty: Check if the SPSCQ is empty.
TYPED_TEST(SPSCQueueTest, Empty) {
    TypeParam q(4);
    EXPECT_TRUE(q.empty());

    EXPECT_TRUE(q.push(1));
    EXPECT_FALSE(q.empty());

    int out = 0;
    EXPECT_TRUE(q.pop(out));
    EXPECT_TRUE(q.empty());
}

// size: Checks the size of the SPSCQ.
TYPED_TEST(SPSCQueueTest, Size) {
    TypeParam q(4);
    EXPECT_EQ(q.size(), 0u);

    EXPECT_TRUE(q.push(1));
    EXPECT_EQ(q.size(), 1u);

    EXPECT_TRUE(q.push(2));
    EXPECT_EQ(q.size(), 2u);

    int out = 0;
    EXPECT_TRUE(q.pop(out));
    EXPECT_EQ(q.size(), 1u);

    EXPECT_TRUE(q.pop(out));
    EXPECT_EQ(q.size(), 0u);
}

// sequencing: Checks guaranteed ordering in a multi-threaded context.
TYPED_TEST(SPSCQueueTest, Sequencing) {
    constexpr int N = 1'000'000;
    const std::size_t capacity =
        next_power_of_two(static_cast<std::size_t>(N) + 1);
    TypeParam q(capacity);

    std::vector<int> received;
    received.reserve(N);

    std::atomic<bool> done{false};

    std::thread consumer([&]() {
        while (!done.load(std::memory_order_acquire) || !q.empty()) {
            int value = 0;
            if (q.pop(value)) {
                received.push_back(value);
            }
        }
    });

    std::thread producer([&]() {
        for (int i = 0; i < N; ++i) {
            while (!q.push(i)) {
                // Spin until there is room.
            }
        }
        done.store(true, std::memory_order_release);
    });

    producer.join();
    consumer.join();

    ASSERT_EQ(received.size(), static_cast<std::size_t>(N));
    for (int i = 0; i < N; ++i) {
        ASSERT_EQ(received[i], i) << "out-of-order element at index " << i;
    }
}

// copy-move: Checks if your queue is movable / copyable.
TYPED_TEST(SPSCQueueTest, CopyMove) {
    static_assert(!std::is_copy_constructible_v<TypeParam>,
                  "queue must not be copy constructible");
    static_assert(!std::is_copy_assignable_v<TypeParam>,
                  "queue must not be copy assignable");
    static_assert(!std::is_move_constructible_v<TypeParam>,
                  "queue must not be move constructible");
    static_assert(!std::is_move_assignable_v<TypeParam>,
                  "queue must not be move assignable");
}

// full: Can't add an item when full.
TYPED_TEST(SPSCQueueTest, Full) {
    TypeParam q(4);  // holds 3 items

    EXPECT_TRUE(q.push(1));
    EXPECT_TRUE(q.push(2));
    EXPECT_TRUE(q.push(3));

    EXPECT_TRUE(q.full());
    EXPECT_FALSE(q.push(4));
    EXPECT_EQ(q.size(), 3u);
}

// exhausted: Checks success in popping from an empty queue.
TYPED_TEST(SPSCQueueTest, Exhausted) {
    TypeParam q(4);

    int out = 42;
    EXPECT_FALSE(q.pop(out));
    EXPECT_EQ(out, 42);  // out param untouched on failure
}

// capacity: Checks the queue honors its configured capacity.
TYPED_TEST(SPSCQueueTest, Capacity) {
    TypeParam q(2);  // holds 1 item

    EXPECT_TRUE(q.push(7));
    EXPECT_TRUE(q.full());
    EXPECT_FALSE(q.push(8));
    EXPECT_EQ(q.size(), 1u);
}

// cannot-write: Capacity of 4, push 5 items.
TYPED_TEST(SPSCQueueTest, CannotWrite) {
    TypeParam q(4);  // holds 3 items

    int accepted = 0;
    for (int i = 0; i < 4; ++i) {
        if (q.push(i)) {
            ++accepted;
        }
    }

    EXPECT_EQ(accepted, 3);
    EXPECT_EQ(q.size(), 3u);
    EXPECT_TRUE(q.full());
}

}  // namespace
