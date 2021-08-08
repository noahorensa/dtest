#include <random.h>

#include <chrono>

double dtest::frand() {
    static thread_local uint32_t __state = std::chrono::high_resolution_clock::now().time_since_epoch().count();

    uint64_t product = (uint64_t) __state * 48271;
    uint32_t x = (product & 0x7fffffff) + (product >> 31);
    __state = x;

    return (double) x / (double) 0x80000000;
}

double dtest::frand_expDist(double lambda) {
    return log(1.0 - frand()) / (-lambda);
}