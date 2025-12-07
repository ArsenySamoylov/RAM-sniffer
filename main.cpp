#include <chrono>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <thread>
#include <sys/mman.h>
#include <unistd.h>

uint64_t now_ms() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(
               steady_clock::now().time_since_epoch())
        .count();
}

//
// transmit_bits()
//  - bits: pointer to array of 0/1 bytes
//  - nbits: number of bits to process
//  - buf_mb: RAM buffer to allocate
//  - bit_ms: how long each bit lasts (ms)
//
//  SAFE VERSION:
//    - "1" = heavy benign RAM sweep
//    - "0" = idle sleep
//
void transmit_bits(const uint8_t *bits,
                   size_t nbits,
                   size_t buf_mb = 512,
                   unsigned bit_ms = 1000)
{
    size_t buf_size = buf_mb * 1024ULL * 1024ULL;

    void *ptr = mmap(nullptr, buf_size,
                     PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

    if (ptr == MAP_FAILED) {
        perror("mmap");
        return;
    }

    // Lock buffer into RAM (best effort)
    mlock(ptr, buf_size);

    volatile uint8_t *buf = (volatile uint8_t *)ptr;

    // Fault all pages in
    for (size_t i = 0; i < buf_size; i += 4096)
        buf[i] = 0xA5;

    const size_t stride = 64;
    std::cout << "transmit_bits: buffer=" << buf_mb
              << " MB, bit duration=" << bit_ms << " ms\n";

    for (size_t b = 0; b < nbits; b++) {
        uint8_t bit = bits[b] & 1;

        uint64_t start = now_ms();
        uint64_t end = start + bit_ms;

        if (bit) {
            std::cout << "Bit " << b << " = 1 (active)\n";
            while (now_ms() < end) {
                for (size_t i = 0; i < buf_size; i += stride)
                    buf[i]++;
            }
        } else {
            std::cout << "Bit " << b << " = 0 (idle)\n";
            std::this_thread::sleep_until(
                std::chrono::steady_clock::time_point{
                    std::chrono::milliseconds(end)});
        }
    }

    munlock(ptr, buf_size);
    munmap(ptr, buf_size);
}

int main() {
    while(true) {
        uint8_t message[] = {1, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0, 0, 0, 1, 0, 1, 0, 1, 1, 0, 0, 0, 1};
        transmit_bits(message, sizeof(message));
        // break;
    }
}
