#include "unordered_map.h"
#include "asserts.h"
#include "error_handler.h"
#include "logger.h"

#include <stdlib.h>
#include <string.h>
#include <stdint.h>

static const size_t INITIAL_CAPACITY = 32;
static const double MAX_LOAD_FACTOR  = 0.7;
static const double MIN_LOAD_FACTOR  = MAX_LOAD_FACTOR / 4.0;

static const uint64_t GOLD_64               = 0x9e3779b97f4a7c15ULL;
static const uint64_t BIG_RANDOM_EVEN_NUM_1 = 0xbf58476d1ce4e5b9ULL;
static const uint64_t BIG_RANDOM_EVEN_NUM_2 = 0x94d049bb133111ebULL;

//================================================================================
//                        Помошники
//================================================================================

static size_t next_pow2_size_t(size_t n) {
    if (n == 0) return 1;
    n--;
    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> 8;
    n |= n >> 16;
    if (sizeof(n) == 8)
        n |= n >> 32;
    return n + 1;
}

static size_t prev_pow2_size_t(size_t n) {
    if (n == 0) return 0;
    size_t p2 = next_pow2_size_t(n);
    return (p2 == n) ? n : (p2 >> 1);
}

static size_t round_up_to(size_t x, size_t align) {
    if (align <= 1) return x;
    size_t rem = x % align;
    return rem == 0 ? x : x + (align - rem);
}

static size_t max_size_t(size_t a, size_t b) { 
    return a > b ? a : b; 
}

static void u_map_calc_layout(size_t  capacity,
                              size_t  key_size,          size_t key_align,
                              size_t  value_size,        size_t value_align,
                              size_t* key_stride_out,    size_t* value_stride_out,
                              size_t* values_offset_out, size_t* states_offset_out,
                              size_t* total_bytes_out) {
    HARD_ASSERT(key_stride_out    != nullptr, "key_stride_out is nullptr");
    HARD_ASSERT(value_stride_out  != nullptr, "value_stride_out is nullptr");
    HARD_ASSERT(values_offset_out != nullptr, "values_offset_out is nullptr");
    HARD_ASSERT(states_offset_out != nullptr, "states_offset_out is nullptr");
    HARD_ASSERT(total_bytes_out   != nullptr, "total_bytes_out is nullptr");

    const size_t key_stride   = round_up_to(key_size,   key_align);
    const size_t value_stride = round_up_to(value_size, value_align);

    const size_t keys_bytes    = capacity * key_stride;
    const size_t values_offset = round_up_to(keys_bytes, value_align);
    const size_t values_bytes  = values_offset + capacity * value_stride;
 
    const size_t states_offset = round_up_to(values_bytes, alignof(elem_state_t));
    const size_t total_bytes   = states_offset + capacity * sizeof(elem_state_t);

    *key_stride_out    = key_stride;
    *value_stride_out  = value_stride;
    *values_offset_out = values_offset;
    *states_offset_out = states_offset;
    *total_bytes_out   = total_bytes;
}

size_t u_map_required_bytes(size_t capacity,
                            size_t key_size,   size_t key_align,
                            size_t value_size, size_t value_align) {
    capacity = prev_pow2_size_t(capacity);
    if (capacity == 0) return 0;

    size_t key_stride = 0, value_stride = 0, values_offset = 0, states_offset = 0, total_bytes = 0;
    u_map_calc_layout(capacity, key_size, key_align, value_size, value_align,
                      &key_stride, &value_stride, &values_offset, &states_offset, &total_bytes);
    return total_bytes;
}

static inline void* get_key(const u_map_t* u_map, size_t index) {
    HARD_ASSERT(u_map != nullptr, "u_map is nullptr");
    HARD_ASSERT(u_map->data_keys != nullptr, "data_keys is nullptr");
    return (void*)((unsigned char*)u_map->data_keys + index * u_map->key_stride);
}

static inline void* get_value(const u_map_t* u_map, size_t index) {
    HARD_ASSERT(u_map != nullptr, "u_map is nullptr");
    HARD_ASSERT(u_map->data_values != nullptr, "data_values is nullptr");
    return (void*)((unsigned char*)u_map->data_values + index * u_map->value_stride);
}

//================================================================================
//                        Хэишрование и проход
//================================================================================

static size_t mix_hash(size_t x) { // splitmix64
    if (sizeof(size_t) != 8) {
        LOGGER_WARNING("size_t is not 64 bits; hashing quality may degrade");
    }
    x += (size_t)GOLD_64;
    x = (x ^ (x >> 30)) * (size_t)BIG_RANDOM_EVEN_NUM_1;
    x = (x ^ (x >> 27)) * (size_t)BIG_RANDOM_EVEN_NUM_2;
    x = x ^ (x >> 31);
    return x;
}

static size_t get_index_and_step(const u_map_t* u_map, const void* key, size_t* step_out) {
    HARD_ASSERT(u_map    != nullptr, "u_map is nullptr");
    HARD_ASSERT(key      != nullptr, "key is nullptr");
    HARD_ASSERT(step_out != nullptr, "step_out is nullptr");
    HARD_ASSERT(u_map->capacity != 0, "capacity is 0");

    size_t raw_hash = u_map->hash_func(key);
    size_t h1 = mix_hash(raw_hash);
    size_t h2 = mix_hash(raw_hash ^ (size_t)GOLD_64);

    size_t mod = u_map->capacity;
    if (mod > 1) *step_out = ((h2 % (mod - 1)) + 1) | 1;  // [1..mod-1], odd
    else         *step_out = 1;

    return h1 % mod;
}

static bool u_map_find_slot(const u_map_t* u_map, const void* key, size_t* idx_out) {
    HARD_ASSERT(u_map   != nullptr, "u_map is nullptr");
    HARD_ASSERT(key     != nullptr, "key is nullptr");
    HARD_ASSERT(idx_out != nullptr, "idx_out is nullptr");

    if (u_map->capacity == 0) return false;

    size_t step = 0;
    size_t start = get_index_and_step(u_map, key, &step);
    size_t idx = start;

    while (u_map->data_states[idx] != EMPTY) {
        if (u_map->data_states[idx] == USED &&
            u_map->key_cmp(get_key(u_map, idx), key)) {
            *idx_out = idx;
            return true;
        }

        idx = (idx + step) % u_map->capacity;
        if (idx == start) break;
    }

    return false;
}

static bool u_map_find_insert_slot(u_map_t* u_map, const void* key, size_t* idx_out, bool* is_new_out) {
    HARD_ASSERT(u_map      != nullptr, "u_map is nullptr");
    HARD_ASSERT(key        != nullptr, "key is nullptr");
    HARD_ASSERT(idx_out    != nullptr, "idx_out is nullptr");
    HARD_ASSERT(is_new_out != nullptr, "is_new_out is nullptr");

    if (u_map->capacity == 0) return false;

    size_t step = 0;
    size_t start = get_index_and_step(u_map, key, &step);
    size_t idx = start;
    size_t first_deleted = (size_t)-1;

    while (u_map->data_states[idx] != EMPTY) {
        if (u_map->data_states[idx] == USED &&
            u_map->key_cmp(get_key(u_map, idx), key)) {
            *idx_out = idx;
            *is_new_out = false;
            return true;
        }

        if (u_map->data_states[idx] == DELETED && first_deleted == (size_t)-1) {
            first_deleted = idx;
        }

        idx = (idx + step) % u_map->capacity;
        if (idx == start) break;
    }

    if (u_map->data_states[idx] == EMPTY) {
        *idx_out = (first_deleted != (size_t)-1) ? first_deleted : idx;
        *is_new_out = true;
        return true;
    }


    if (first_deleted != (size_t)-1) {
        *idx_out = first_deleted;
        *is_new_out = true;
        return true;
    }

    return false;
}

//================================================================================
//                        Рехэш и нормализация
//================================================================================

static error_t u_map_rehash(u_map_t* u_map, size_t new_capacity) {
    HARD_ASSERT(u_map != nullptr, "u_map is nullptr");

    if (u_map->is_static) return HM_ERR_OK;

    if (new_capacity < INITIAL_CAPACITY)
        new_capacity = INITIAL_CAPACITY;

    new_capacity = next_pow2_size_t(new_capacity);

    u_map_t new_map;
    memset(&new_map, 0, sizeof(new_map));
    error_t err = u_map_init(&new_map, new_capacity,
                             u_map->key_size,   u_map->key_align,
                             u_map->value_size, u_map->value_align,
                             u_map->hash_func, u_map->key_cmp);
    RETURN_IF_ERROR(err);

    for (size_t i = 0; i < u_map->capacity; ++i) {
        if (u_map->data_states[i] != USED) continue;

        const void* key   = get_key(u_map, i);
        const void* value = get_value(u_map, i);

        bool is_new = false;
        size_t idx = 0;
        bool is_not_full = u_map_find_insert_slot(&new_map, key, &idx, &is_new);
        if (!is_not_full) {
            u_map_dest(&new_map);
            return HM_ERR_FULL;
        }

        new_map.data_states[idx] = USED;
        memcpy(get_key  (&new_map, idx), key,   u_map->key_size);
        memcpy(get_value(&new_map, idx), value, u_map->value_size);
        new_map.size++;
        new_map.occupied++;
    }

    free(u_map->data);
    *u_map = new_map;
    return HM_ERR_OK;
}

static error_t normalize_capacity(u_map_t* u_map) {
    HARD_ASSERT(u_map != nullptr, "u_map is nullptr");

    if (u_map->is_static || u_map->capacity == 0)
        return HM_ERR_OK;

    double load_occupied = (double)u_map->occupied / (double)u_map->capacity;
    double load_real     = (double)u_map->size     / (double)u_map->capacity;

    size_t new_capacity = u_map->capacity;

    if (load_occupied > MAX_LOAD_FACTOR) {
        new_capacity = u_map->capacity * 2;
    } else if (u_map->capacity > INITIAL_CAPACITY && load_real < MIN_LOAD_FACTOR) {
        new_capacity = u_map->capacity / 2;
        if (new_capacity < INITIAL_CAPACITY)
            new_capacity = INITIAL_CAPACITY;
    }

    if (new_capacity == u_map->capacity)
        return HM_ERR_OK;

    LOGGER_DEBUG("Changing capacity from %zu to %zu", u_map->capacity, new_capacity);
    return u_map_rehash(u_map, new_capacity);
}

//================================================================================
//                       Конструкторы / Деструкторы / Копировальщики
//================================================================================

error_t u_map_init(u_map_t* u_map, size_t capacity,
                   size_t key_size,   size_t key_align,
                   size_t value_size, size_t value_align,
                   key_func_t hash_func, key_cmp_t key_cmp) {

    HARD_ASSERT(u_map      != nullptr, "u_map is nullptr");
    HARD_ASSERT(hash_func  != nullptr, "hash_func is nullptr");
    HARD_ASSERT(key_cmp    != nullptr, "key_cmp is nullptr");

    LOGGER_DEBUG("u_map_init started");

    if (capacity < INITIAL_CAPACITY) capacity = INITIAL_CAPACITY;
    capacity = next_pow2_size_t(capacity);

    size_t key_stride = 0, value_stride = 0, values_offset = 0, states_offset = 0, total_bytes = 0;
    u_map_calc_layout(capacity, key_size, key_align, value_size, value_align,
                      &key_stride, &value_stride, &values_offset, &states_offset, &total_bytes);

    void* data = calloc(1, total_bytes);
    if (!data) return HM_ERR_MEM_ALLOC;

    u_map->data        = data;
    u_map->data_keys   = data;
    u_map->data_values = (void*)((unsigned char*)data + values_offset);
    u_map->data_states = (elem_state_t*)((unsigned char*)data + states_offset);

    u_map->size      = 0;
    u_map->occupied  = 0;
    u_map->capacity  = capacity;

    u_map->key_size   = key_size;
    u_map->key_align  = key_align;
    u_map->key_stride = key_stride;

    u_map->value_size   = value_size;
    u_map->value_align  = value_align;
    u_map->value_stride = value_stride;

    u_map->hash_func = hash_func;
    u_map->key_cmp   = key_cmp;

    u_map->is_static = false;

    return HM_ERR_OK;
}

error_t u_map_static_init(u_map_t* u_map, void* data, size_t capacity,
                          size_t key_size,   size_t key_align,
                          size_t value_size, size_t value_align,
                          key_func_t hash_func, key_cmp_t key_cmp) {

    HARD_ASSERT(u_map      != nullptr, "u_map is nullptr");
    HARD_ASSERT(data       != nullptr, "data is nullptr");
    HARD_ASSERT(hash_func  != nullptr, "hash_func is nullptr");
    HARD_ASSERT(key_cmp    != nullptr, "key_cmp is nullptr");
    HARD_ASSERT(key_size   > 0,     "key_size must be > 0");
    HARD_ASSERT(value_size > 0,     "value_size must be > 0");
    HARD_ASSERT(key_align  > 0,     "key_align must be > 0");
    HARD_ASSERT(value_align> 0,     "value_align must be > 0");

    LOGGER_DEBUG("u_map_static_init started");

    capacity = prev_pow2_size_t(capacity);
    RETURN_IF_ERROR(capacity == 0 ? HM_ERR_BAD_ARG : HM_ERR_OK);

    size_t key_stride = 0, value_stride = 0, values_offset = 0, states_offset = 0, total_bytes = 0;
    u_map_calc_layout(capacity, key_size, key_align, value_size, value_align,
                      &key_stride, &value_stride, &values_offset, &states_offset, &total_bytes);

    size_t need_align = max_size_t(max_size_t(key_align, value_align), alignof(elem_state_t));
    if (((uintptr_t)data % need_align) != 0) {
        LOGGER_ERROR("static buffer is not aligned to %zu bytes", need_align);
        return HM_ERR_BAD_ARG;
    }

    memset((unsigned char*)data + states_offset, 0, capacity * sizeof(elem_state_t));

    u_map->data        = data;
    u_map->data_keys   = data;
    u_map->data_values = (void*)((unsigned char*)data + values_offset);
    u_map->data_states = (elem_state_t*)((unsigned char*)data + states_offset);

    u_map->size     = 0;
    u_map->occupied = 0;
    u_map->capacity = capacity;

    u_map->key_size   = key_size;
    u_map->key_align  = key_align;
    u_map->key_stride = key_stride;

    u_map->value_size   = value_size;
    u_map->value_align  = value_align;
    u_map->value_stride = value_stride;

    u_map->hash_func = hash_func;
    u_map->key_cmp   = key_cmp;

    u_map->is_static = true;

    return HM_ERR_OK;
}

error_t u_map_destroy(u_map_t* u_map) {
    HARD_ASSERT(u_map != nullptr, "u_map is nullptr");

    LOGGER_DEBUG("u_map_dest started");

    if (!u_map->is_static && u_map->data != nullptr) {
        free(u_map->data);
    }

    memset(u_map, 0, sizeof(*u_map));
    return HM_ERR_OK;
}

error_t u_map_smart_copy(u_map_t* target, const u_map_t* source) {
    HARD_ASSERT(target != nullptr, "target is nullptr");
    HARD_ASSERT(source != nullptr, "source is nullptr");

    LOGGER_DEBUG("u_map_smart_copy started");

    error_t err = u_map_init(target, source->capacity,
                             source->key_size,   source->key_align,
                             source->value_size, source->value_align,
                             source->hash_func, source->key_cmp);
    RETURN_IF_ERROR(err);

    for (size_t i = 0; i < source->capacity; ++i) {
        if (source->data_states[i] != USED) continue;

        const void* key   = get_key(source, i);
        const void* value = get_value(source, i);

        bool is_new = false;
        size_t idx = 0;
        bool is_not_full = u_map_find_insert_slot(target, key, &idx, &is_new);
        if (!is_not_full) {
            u_map_dest(target);
            return HM_ERR_FULL;
        }

        target->data_states[idx] = USED;
        memcpy(get_key  (target, idx), key,   source->key_size);
        memcpy(get_value(target, idx), value, source->value_size);
        target->size++;
        target->occupied++;
    }

    return HM_ERR_OK;
}

error_t u_map_raw_copy(u_map_t* target, const u_map_t* source) {
    HARD_ASSERT(target != nullptr, "target is nullptr");
    HARD_ASSERT(source != nullptr, "source is nullptr");

    LOGGER_DEBUG("u_map_raw_copy started");

    error_t err = u_map_init(target, source->capacity,
                             source->key_size,   source->key_align,
                             source->value_size, source->value_align,
                             source->hash_func, source->key_cmp);
    RETURN_IF_ERROR(err);

    size_t total_bytes = u_map_required_bytes(source->capacity,
                                              source->key_size, source->key_align,
                                              source->value_size, source->value_align);
    memcpy(target->data, source->data, total_bytes);

    target->size     = source->size;
    target->occupied = source->occupied;

    return HM_ERR_OK;
}

//================================================================================
//                             Базовые функции
//================================================================================

bool u_map_is_empty(const u_map_t* u_map) {
    HARD_ASSERT(u_map != nullptr, "u_map is nullptr");
    return u_map->size == 0;
}

size_t u_map_size(const u_map_t* u_map) {
    HARD_ASSERT(u_map != nullptr, "u_map is nullptr");
    return u_map->size;
}

size_t u_map_capacity(const u_map_t* u_map) {
    HARD_ASSERT(u_map != nullptr, "u_map is nullptr");
    return u_map->capacity;
}

bool u_map_get_elem(const u_map_t* u_map, const void* key, void* value_out) {
    HARD_ASSERT(u_map != nullptr, "u_map is nullptr");
    HARD_ASSERT(key   != nullptr, "key is nullptr");

    size_t idx = 0;
    if (!u_map_find_slot(u_map, key, &idx)) {
        return false;
    }

    if (value_out != nullptr) {
        memcpy(value_out, get_value(u_map, idx), u_map->value_size);
    }
    return true;
}

error_t u_map_insert_elem(u_map_t* u_map, const void* key, const void* value) {
    HARD_ASSERT(u_map  != nullptr, "u_map is nullptr");
    HARD_ASSERT(key    != nullptr, "key is nullptr");
    HARD_ASSERT(value  != nullptr, "value is nullptr");

    LOGGER_DEBUG("u_map_insert_elem started");

    error_t err = normalize_capacity(u_map);
    RETURN_IF_ERROR(err);

    size_t idx = 0;
    bool is_new = false;
    if (!u_map_find_insert_slot(u_map, key, &idx, &is_new)) {
        return HM_ERR_FULL;
    }

    if (!is_new) {
        memcpy(get_value(u_map, idx), value, u_map->value_size);
        return HM_ERR_OK;
    }

    if (u_map->data_states[idx] == EMPTY) {
        u_map->occupied++;
        u_map->size++;
    } else if (u_map->data_states[idx] == DELETED) {
        u_map->size++;
    }

    u_map->data_states[idx] = USED;
    memcpy(get_key  (u_map, idx), key,   u_map->key_size);
    memcpy(get_value(u_map, idx), value, u_map->value_size);

    return HM_ERR_OK;
}

error_t u_map_remove_elem(u_map_t* u_map, const void* key, void* value_out) {
    HARD_ASSERT(u_map != nullptr, "u_map is nullptr");
    HARD_ASSERT(key   != nullptr, "key is nullptr");

    LOGGER_DEBUG("u_map_remove_elem started");

    error_t err = normalize_capacity(u_map);
    RETURN_IF_ERROR(err);

    size_t idx = 0;
    if (!u_map_find_slot(u_map, key, &idx)) {
        return HM_ERR_NOT_FOUND;
    }

    if (value_out != nullptr) {
        memcpy(value_out, get_value(u_map, idx), u_map->value_size);
    }

    u_map->data_states[idx] = DELETED;
    u_map->size--;

    return HM_ERR_OK;
}

//================================================================================
//                              Продвинутые
//================================================================================

error_t read_arr_to_u_map(u_map_t* u_map, const void* arr, size_t pair_count) {
    HARD_ASSERT(u_map != nullptr, "u_map is nullptr");
    HARD_ASSERT(arr  != nullptr || pair_count == 0, "arr is nullptr");

    const unsigned char* ptr = (const unsigned char*)arr;

    size_t key_part    = u_map->key_size;
    size_t value_off   = round_up_to(key_part, u_map->value_align);
    size_t pair_stride = value_off + u_map->value_size;

    for (size_t i = 0; i < pair_count; ++i) {
        const void* key   = (const void*)(ptr + i * pair_stride);
        const void* value = (const void*)(ptr + i * pair_stride + value_off);

        error_t err = u_map_insert_elem(u_map, key, value);
        RETURN_IF_ERROR(err);
    }

    return HM_ERR_OK;
}
