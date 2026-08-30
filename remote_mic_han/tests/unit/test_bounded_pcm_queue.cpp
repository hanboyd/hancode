// Phase 4 / ADR-0014 §3.1: BoundedPcmQueue TDD red-state tests.
//
// Stub behavior: push() silently discards; pop_up_to() returns empty;
// dropped_count_ stays 0. Tests below assert the real contract. They
// FAIL on the stub; step 2 turns them green.

#include "remotemic/audio/bounded_pcm_queue.hpp"

#include <iostream>
#include <span>
#include <vector>

namespace {

using remotemic::audio::BoundedPcmQueue;

int failures = 0;

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void test_push_retains_samples_in_order() {
    BoundedPcmQueue q(/*capacity_samples=*/100);
    std::vector<std::int16_t> in{1, 2, 3, 4, 5};
    q.push(std::span<const std::int16_t>(in));
    expect(q.size() == 5, "push(5 samples) -> size() == 5");
    auto popped = q.pop_up_to(10);
    expect(popped.size() == 5, "pop_up_to returns all 5 samples");
    expect(popped[0] == 1 && popped[4] == 5,
           "popped samples are in FIFO order");
    expect(q.size() == 0, "queue is empty after pop_up_to");
}

void test_pop_up_to_caps_returned_count() {
    BoundedPcmQueue q(/*capacity_samples=*/100);
    std::vector<std::int16_t> in{10, 20, 30, 40, 50};
    q.push(std::span<const std::int16_t>(in));
    auto popped = q.pop_up_to(3);
    expect(popped.size() == 3, "pop_up_to(3) returns 3 samples");
    expect(popped[0] == 10 && popped[2] == 30,
           "pop_up_to(3) returns the 3 oldest samples");
    auto popped2 = q.pop_up_to(10);
    expect(popped2.size() == 2, "remaining 2 samples returned in second call");
    expect(popped2[0] == 40 && popped2[1] == 50,
           "second pop returns the next 2 samples in order");
}

void test_overflow_drops_oldest_and_counts_them() {
    BoundedPcmQueue q(/*capacity_samples=*/4);
    std::vector<std::int16_t> first{1, 2, 3, 4};
    q.push(std::span<const std::int16_t>(first));
    expect(q.size() == 4, "first push fills the queue");
    expect(q.dropped_count() == 0,
           "dropped_count is 0 when no overflow happened");

    std::vector<std::int16_t> second{5, 6, 7};
    q.push(std::span<const std::int16_t>(second));
    expect(q.size() == 4, "queue remains at capacity after overflow");
    expect(q.dropped_count() == 3,
           "3 oldest samples were dropped to make room");

    auto popped = q.pop_up_to(10);
    expect(popped.size() == 4, "4 samples remain after overflow");
    expect(popped[0] == 4 && popped[1] == 5 && popped[2] == 6 && popped[3] == 7,
           "surviving samples are {4, 5, 6, 7} (oldest three were evicted)");
}

void test_zero_capacity_throws() {
    bool threw = false;
    try {
        BoundedPcmQueue q(0);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    expect(threw, "BoundedPcmQueue(0) throws std::invalid_argument");
}

void test_push_empty_span_is_noop() {
    BoundedPcmQueue q(/*capacity_samples=*/10);
    std::vector<std::int16_t> empty{};
    std::size_t dropped = q.push(std::span<const std::int16_t>(empty));
    expect(dropped == 0, "push(empty span) drops 0 samples");
    expect(q.size() == 0, "queue stays empty after empty push");
    expect(q.dropped_count() == 0, "dropped_count stays 0 after empty push");
}

}  // namespace

int main() {
    test_push_retains_samples_in_order();
    test_pop_up_to_caps_returned_count();
    test_overflow_drops_oldest_and_counts_them();
    test_zero_capacity_throws();
    test_push_empty_span_is_noop();

    if (failures != 0) {
        std::cerr << "BoundedPcmQueue tests: " << failures
                  << " failure(s) (red state on stub; step 2 turns green)\n";
        return 1;
    }
    std::cout << "All BoundedPcmQueue tests passed\n";
    return 0;
}