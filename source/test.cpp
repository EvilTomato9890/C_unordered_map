#include <memory.h>

#include "unordered_map.h"
#include "logger.h"

static size_t key_hash(const void* a) {
    return *(const size_t*)a;
}

static bool key_cmp(const void* a, const void* b) {
    return *(const size_t*)a == *(const size_t*)b;
}


int main() {
    logger_initialize_stream(nullptr);

    u_map_t map = {};
    SIMPLE_U_MAP_INIT(&map, 2, size_t, char, key_hash, key_cmp);
    char   val = 'c';
    size_t key = 500;
    u_map_insert_elem(&map, (void*)&key, (void*)&val);
    char ans = 0;
    u_map_remove_elem(&map, (void*)&key, (void*)&ans);
    LOGGER_DEBUG("AA: %c", ans);
    u_map_dest(&map);
}