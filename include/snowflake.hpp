#pragma once
#include <cstdint>
#include <chrono>
#include <mutex>
#include <stdexcept>
#include "lib.hpp"

// The Snowflake ID generator class.
class SnowflakeIdGenerator {
public:
    // Constructor initializes the worker and data center IDs.
    // Throws an exception if the IDs are out of the valid range.
    SnowflakeIdGenerator(int workerId, int datacenterId)
        : workerId_(workerId), datacenterId_(datacenterId), lastTimestamp_(1) {
        if (workerId_ > MAX_WORKER_ID || workerId_ < 0) {
            THROW("Worker ID must be between 0 and 31");
        }
        if (datacenterId_ > MAX_DATACENTER_ID || datacenterId_ < 0) {
            THROW("Datacenter ID must be between 0 and 31");
        }
    }

    // Generates a new, unique Snowflake ID.
    uint64_t get_id() {
        std::lock_guard<std::mutex> lock(mutex_);

        // Get the current timestamp in milliseconds.
        uint64_t timestamp = getTimestamp();

        // Check if the clock has moved backwards.
        if (timestamp < lastTimestamp_) {
            THROW("Clock moved backwards. Refusing to generate ID for " + std::to_string(lastTimestamp_ - timestamp) + " milliseconds");
        }

        // If we are in the same millisecond as the last ID, increment the sequence.
        if (timestamp == lastTimestamp_) {
            sequence_ = (sequence_ + 1) & SEQUENCE_MASK;
            if (sequence_ == 0) {
                // If sequence overflows, wait until the next millisecond.
                timestamp = waitNextMillis(lastTimestamp_);
            }
        } else {
            // If the time has moved forward, reset the sequence.
            sequence_ = 0;
        }

        // Update the last timestamp.
        lastTimestamp_ = timestamp;

        // Use bitwise left shifts to construct the 64-bit ID.
        // 1. Shift the timestamp (41 bits) to the left to occupy the most significant bits.
        // 2. Shift the datacenter ID (5 bits) next.
        // 3. Shift the worker ID (5 bits) next.
        // 4. Combine with the sequence number (12 bits) at the end.
        uint64_t id = ((timestamp - EPOCH) << TIMESTAMP_SHIFT) |
                      (datacenterId_ << DATACENTER_ID_SHIFT) |
                      (workerId_ << WORKER_ID_SHIFT) |
                      sequence_;

        return id;
    }

private:
    // All constants are private and static.
    static constexpr uint64_t EPOCH = 1288834974657; // A custom epoch (e.g., Twitter's).
    static constexpr int SEQUENCE_BITS = 12;
    static constexpr int WORKER_ID_BITS = 5;
    static constexpr int DATACENTER_ID_BITS = 5;

    // The maximum values for worker and datacenter IDs, based on the bit allocation.
    static constexpr int MAX_WORKER_ID = (1 << WORKER_ID_BITS) - 1;
    static constexpr int MAX_DATACENTER_ID = (1 << DATACENTER_ID_BITS) - 1;
    static constexpr int SEQUENCE_MASK = (1 << SEQUENCE_BITS) - 1;

    // Bit shifts for each component of the ID.
    static constexpr int WORKER_ID_SHIFT = SEQUENCE_BITS;
    static constexpr int DATACENTER_ID_SHIFT = SEQUENCE_BITS + WORKER_ID_BITS;
    static constexpr int TIMESTAMP_SHIFT = SEQUENCE_BITS + WORKER_ID_BITS + DATACENTER_ID_BITS;

    // Member variables to store the state of the generator.
    int workerId_;
    int datacenterId_;
    uint64_t sequence_ = 0;
    uint64_t lastTimestamp_ = 1;

    // A mutex for thread-safe generation.
    std::mutex mutex_;

    // Gets the current timestamp in milliseconds.
    uint64_t getTimestamp() const {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();
    }

    // Waits until the next millisecond to avoid ID collisions.
    uint64_t waitNextMillis(uint64_t lastTimestamp) const {
        uint64_t timestamp = getTimestamp();
        while (timestamp <= lastTimestamp) {
            timestamp = getTimestamp();
        }
        return timestamp;
    }
};

