#include "unordered_map.h"
#include "logger.h"

static size_t value_hash(const void* a) {
    return *(const size_t*)a;
}

static bool key_cmp(const void* a, const void* b) {
    return *(const char*)a == *(const char*)b;
}

int main() {
    logger_initialize_stream(nullptr);

    unordered_map_t map = {};
    unordered_map_init(&map, 14, sizeof(char), sizeof(size_t), value_hash, key_cmp);
    
    unordered_map_dest(&map);
}