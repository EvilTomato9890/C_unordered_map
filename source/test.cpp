#include <memory.h>

#include "unordered_map.h"
#include "logger.h"

static size_t key_hash(const void* a) {
    size_t x;
    memcpy(&x, a, sizeof x);
    return x;
}

static bool key_cmp(const void* a, const void* b) {
    size_t x, y;
    memcpy(&x, a, sizeof x);
    memcpy(&y, b, sizeof y);
    return x == y;
}


int main() {
    logger_initialize_stream(nullptr);

    u_map_t map = {};
    u_map_init(&map, 14, sizeof(size_t), sizeof(char), key_hash, key_cmp);
    char   val = 'c';
    size_t key = 500;
    u_map_insert_elem(&map, (void*)&key, (void*)&val);
    char ans = 0;
    u_map_remove_elem(&map, (void*)&key, (void*)&ans);
    LOGGER_DEBUG("AA: %c", ans);
    u_map_dest(&map);
}