#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/mman.h>

uint64_t now_ms(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (uint64_t)t.tv_sec * 1000 + t.tv_nsec / 1000000;
}

int main(int argc, char **argv) {
    size_t buf_mb = 5555;           // default buffer size
    unsigned active_ms = 10000;     // default active duration (ms)
    unsigned idle_ms = 5000;       // default idle duration (ms)

    if (argc > 1) buf_mb = strtoull(argv[1], NULL, 0);
    if (argc > 2) active_ms = (unsigned)strtoul(argv[2], NULL, 0);
    if (argc > 3) idle_ms = (unsigned)strtoul(argv[3], NULL, 0);

    size_t buf_size = buf_mb * 1024ULL * 1024ULL;

    void *ptr = mmap(NULL, buf_size, PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (ptr == MAP_FAILED) {
        perror("mmap");
        return 1;
    }

    // Try to lock into RAM to avoid swapping (best-effort)
    if (mlock(ptr, buf_size) != 0) {
        fprintf(stderr, "mlock failed: %s (continuing)\n", strerror(errno));
    }

    // initialize to ensure pages are faulted in
    volatile char *buf = (volatile char *)ptr;
    for (size_t i = 0; i < buf_size; i += 4096) buf[i] = 0xA5;

    printf("mem_blink: buffer=%zu MB, active=%ums, idle=%ums\n",
           buf_mb, active_ms, idle_ms);
    printf("Press Ctrl-C to stop\n");

    const size_t stride = 64; // touch every cache line (approx)
    while (1) {
        printf("Start writing\n"); 
        uint64_t start = now_ms();
        uint64_t deadline = start + active_ms;

        while (now_ms() < deadline) {
           for (size_t i = 0; i < buf_size; i += stride) {
                buf[i] = (char)(buf[i] + 1);
            }
        }
        
        printf("Sleeping\n");
        struct timespec ts;
        ts.tv_sec = idle_ms / 1000;
        ts.tv_nsec = (idle_ms % 1000) * 1000000;
        nanosleep(&ts, NULL);
    }
    
    return 0;
}
