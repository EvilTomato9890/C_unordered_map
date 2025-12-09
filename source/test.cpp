#include "unordered_map.h"
#include "logger.h"

static size_t key_hash(const void* a) {
    return *(const size_t*)a;
}

static bool key_cmp(const void* a, const void* b) {
    return *(const unsigned char*)a == *(const unsigned char*)b;
}


int main() {
    logger_initialize_stream(nullptr);

    unordered_map_t map = {};
    unordered_map_init(&map, 14, sizeof(size_t), sizeof(char), key_hash, key_cmp);
    char   val = 'c';
    size_t key = 5;
    insert_elem(&map, (void*)&key, (void*)&val);
    size_t ans = 0;
    remove_elem(&map, (void*)&key, (void*)&ans);
    LOGGER_DEBUG("AA: %zu", ans);
    unordered_map_dest(&map);
}