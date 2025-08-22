#pragma once
#include <chrono>
#include <random>
#include <string>
#include <sstream>
#include <iomanip>

class ULID {
public:
    // Generate a new ULID string (Crockford's Base32, 26 chars)
    static std::string get_id() {
        // 48 bits timestamp (ms since Unix epoch)
        auto now = std::chrono::system_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
        uint64_t timestamp = static_cast<uint64_t>(ms);

        // 80 bits randomness
        std::random_device rd;
        std::mt19937_64 gen(rd());
        uint64_t rand_hi = gen();
        uint16_t rand_lo = static_cast<uint16_t>(gen());

        uint8_t bytes[16] = {};
        // 48 bits timestamp
        bytes[0] = (timestamp >> 40) & 0xFF;
        bytes[1] = (timestamp >> 32) & 0xFF;
        bytes[2] = (timestamp >> 24) & 0xFF;
        bytes[3] = (timestamp >> 16) & 0xFF;
        bytes[4] = (timestamp >> 8) & 0xFF;
        bytes[5] = (timestamp >> 0) & 0xFF;
        // 80 bits random
        bytes[6] = (rand_hi >> 56) & 0xFF;
        bytes[7] = (rand_hi >> 48) & 0xFF;
        bytes[8] = (rand_hi >> 40) & 0xFF;
        bytes[9] = (rand_hi >> 32) & 0xFF;
        bytes[10] = (rand_hi >> 24) & 0xFF;
        bytes[11] = (rand_hi >> 16) & 0xFF;
        bytes[12] = (rand_hi >> 8) & 0xFF;
        bytes[13] = (rand_hi >> 0) & 0xFF;
        bytes[14] = (rand_lo >> 8) & 0xFF;
        bytes[15] = (rand_lo >> 0) & 0xFF;

        // Crockford Base32 alphabet
        static const char* CROCKFORD = "0123456789ABCDEFGHJKMNPQRSTVWXYZ";

        char ulid[27] = {};
        int bit = 0;
        for (int i = 0; i < 26; ++i) {
            int idx = 0;
            for (int j = 0; j < 5; ++j) {
                idx <<= 1;
                int byte_pos = (bit + j) / 8;
                int bit_pos = 7 - ((bit + j) % 8);
                idx |= (bytes[byte_pos] >> bit_pos) & 0x01;
            }
            ulid[i] = CROCKFORD[idx];
            bit += 5;
        }
        ulid[26] = '\0';
        return std::string(ulid);
    }
};
