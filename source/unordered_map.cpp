#include "unordered_map.h"
#include "asserts.h"
#include "error_handler.h"
#include "logger.h"

#include <stdlib.h>
#include <stdio.h>
#include <memory.h>
#include <stdint.h>

const size_t INITIAL_CAPACITY = 16;
const double MAX_LOAD_FACTOR  = 0.7;
const int MAX_TYPE_SIZE_BEFORE_CALLOC = 512;

const uint64_t GOLD_64               = 0x9e3779b97f4a7c15ULL;
const uint64_t BIG_RANDOM_EVEN_NUM_1 = 0xbf58476d1ce4e5b9ULL;
const uint64_t BIG_RANDOM_EVEN_NUM_2 = 0x94d049bb133111ebULL;
//================================================================================
//                        Помошники функции
//================================================================================

static size_t next_pow2_size_t(size_t n) {
    if (n == 0) return 1;
    n--;
    n |= n >> 1;         
    n |= n >> 2;         
    n |= n >> 4;         
    n |= n >> 8;         
    n |= n >> 16;  
    if(sizeof(n) == 8)             
        n |= n >> 32;
    
    n++;            
    return n;
}

static size_t prev_pow2_size_t(size_t n) {
    if (n == 0) return 0;
    size_t new_pow2 = next_pow2_size_t(n);
    if (new_pow2 == n) return n;
    return n - (n >> 1);            
}

static bool is_pow2(size_t n) {
    return n != 0 && (n & (n - 1)) == 0;
}


//================================================================================
//                        Встроенные функции
//================================================================================

static size_t mix_hash(size_t x) { //splitmix64
    if(sizeof(size_t) != 8) LOGGER_WARNING("size_t is not 64 bits");
    x += (size_t)GOLD_64;
    x = (x ^ (x >> 30)) * (size_t)BIG_RANDOM_EVEN_NUM_1;
    x = (x ^ (x >> 27)) * (size_t)BIG_RANDOM_EVEN_NUM_2;
    x = x ^ (x >> 31);
    return x;
}

static size_t get_index_and_step(const u_map_t* u_map, const void* key, size_t* step_out) {
    HARD_ASSERT(u_map != nullptr, "u_map is nullptr");
    HARD_ASSERT(key           != nullptr, "Key is nullptr");
    HARD_ASSERT(step_out      != nullptr, "Step_out is nullptr");    
    HARD_ASSERT(u_map->capacity != 0, "Capacity is 0");

    size_t raw_hash = u_map->hash_func(key);
    size_t hash_1   = mix_hash(raw_hash);
    size_t hash_2   = mix_hash(raw_hash ^ (size_t)GOLD_64);

    size_t mod = u_map->capacity;
    if (mod > 1) *step_out = (hash_2 % (mod - 1)) | 1;
    else         *step_out = 1;
    return hash_1 % mod;

}

static error_t normalize_capacity(u_map_t* u_map) {
    return HM_ERR_OK;
}

//================================================================================
//                        Консрукторы, деконструкторы, копировальщики
//================================================================================

error_t u_map_init(u_map_t* u_map, size_t capacity, 
                   size_t key_size, size_t value_size,
                   key_func_t hash_func, key_cmp_t key_cmp) {
    HARD_ASSERT(u_map != nullptr, "u_map pointer is nullptr");
    HARD_ASSERT(hash_func     != nullptr, "hash_func pointer is nullptr");
    HARD_ASSERT(key_cmp       != nullptr, "key_cmp pointer is nullptr");

    LOGGER_DEBUG("u_map_init started");
    if(capacity < INITIAL_CAPACITY) capacity = INITIAL_CAPACITY;
    else                            capacity = next_pow2_size_t(capacity);
    LOGGER_DEBUG("New capacity is: %zu", capacity);

    u_map->data       = calloc(capacity, key_size + value_size);
    RETURN_IF_ERROR(u_map->data == nullptr ? HM_ERR_MEM_ALLOC : HM_ERR_OK);
    u_map->slots      = (elem_t*)calloc(capacity, sizeof(elem_t));
    RETURN_IF_ERROR(u_map->slots == nullptr ? HM_ERR_MEM_ALLOC : HM_ERR_OK, free(u_map->data););

    for(size_t i = 0; i < capacity; i++) {
        u_map->slots[i].key   = (char*)u_map->data + i * (key_size + value_size);
        u_map->slots[i].value = (char*)u_map->data + i * (key_size + value_size) + key_size;
        u_map->slots[i].state = EMPTY;
    }

    u_map->size       = 0;
    u_map->capacity   = capacity;
    u_map->value_size = value_size;
    u_map->key_size   = key_size;
    u_map->hash_func  = hash_func;
    u_map->key_cmp    = key_cmp;
    u_map->is_static  = false;
    return HM_ERR_OK;
}

error_t u_map_static_init(u_map_t* u_map, void* data, 
                          size_t capacity, size_t key_size, size_t value_size,
                          key_func_t hash_func, key_cmp_t key_cmp) {
    HARD_ASSERT(u_map != nullptr, "u_map pointer is nullptr");
    HARD_ASSERT(hash_func     != nullptr, "hash_func pointer is nullptr");
    HARD_ASSERT(key_cmp       != nullptr, "key_cmp pointer is nullptr");

    LOGGER_DEBUG("u_map_static_init started");
    capacity = prev_pow2_size_t(capacity);
    LOGGER_DEBUG("New capacity is: %zu", capacity);

    u_map->slots = (elem_t*)calloc(capacity, sizeof(elem_t));
    RETURN_IF_ERROR(u_map->slots == nullptr ? HM_ERR_MEM_ALLOC : HM_ERR_OK);

    u_map->data = data;
    for(size_t i = 0; i < capacity; i++) {
        u_map->slots[i].key   = (char*)u_map->data + i * (key_size + value_size);
        u_map->slots[i].value = (char*)u_map->data + i * (key_size + value_size) + key_size;
        u_map->slots[i].state = EMPTY;
    }

    u_map->size       = 0;
    u_map->capacity   = capacity;
    u_map->value_size = value_size;
    u_map->key_size   = key_size;
    u_map->hash_func  = hash_func;
    u_map->key_cmp    = key_cmp;
    u_map->is_static  = true;
    return HM_ERR_OK;
}

error_t u_map_dest(u_map_t* u_map) {
    HARD_ASSERT(u_map != nullptr, "u_map pointer is nullptr");

    LOGGER_DEBUG("u_map_dest started");
    free(u_map->slots);
    if(!u_map->is_static) {
        free(u_map->data);
    }
    u_map->slots = nullptr;
    u_map->data  = nullptr;
    u_map->size  = 0;
    u_map->capacity = 0;
    return HM_ERR_OK;
}

error_t u_map_smart_copy(const u_map_t* source, u_map_t* target) {
    HARD_ASSERT(source != nullptr, "source u_map pointer is nullptr");
    HARD_ASSERT(target != nullptr, "target u_map pointer is nullptr");

    LOGGER_DEBUG("u_map_smart_copy started");

    error_t err = u_map_init(target, source->capacity,
                             source->key_size,  source->value_size,
                             source->hash_func, source->key_cmp);
    RETURN_IF_ERROR(err);

    target->size = 0;

    for (size_t i = 0; i < source->capacity; ++i) {
        if (source->slots[i].state != USED)
            continue;

        const void* key   = source->slots[i].key;
        const void* value = source->slots[i].value;

        size_t step = 0;
        size_t idx  = get_index_and_step(target, key, &step);

        while (target->slots[idx].state == USED) {
            idx = (idx + step) % target->capacity;
        }

        memcpy(target->slots[idx].key,   key,   source->key_size);
        memcpy(target->slots[idx].value, value, source->value_size);
        target->slots[idx].state = USED;
        target->size++;
    }

    return HM_ERR_OK;
}

error_t u_map_raw_copy(const u_map_t* source, u_map_t* target) {
    HARD_ASSERT(source != nullptr, "source u_map pointer is nullptr");
    HARD_ASSERT(target != nullptr, "target u_map pointer is nullptr");

    LOGGER_DEBUG("u_map_raw_copy started");

    error_t err = u_map_init(target, source->capacity,
                             source->key_size,  source->value_size,
                             source->hash_func, source->key_cmp);
    RETURN_IF_ERROR(err);

    const size_t elem_stride = source->key_size + source->value_size;
    const size_t total_bytes = source->capacity * elem_stride;

    memcpy(target->data, source->data, total_bytes);

    for (size_t i = 0; i < source->capacity; ++i) {
        target->slots[i].state = source->slots[i].state;
    }

    target->size = 0;
    for (size_t i = 0; i < target->capacity; ++i) {
        if (target->slots[i].state == USED)
            target->size++;
    }

    return HM_ERR_OK;
}



//================================================================================
//                              Базовые функции
//================================================================================

bool u_map_is_empty(const u_map_t* u_map) {
    HARD_ASSERT(u_map != nullptr, "u_map pointer is nullptr");
    return u_map->size == 0;
}

size_t u_map_size    (const u_map_t* u_map) {
    HARD_ASSERT(u_map != nullptr, "u_map pointer is nullptr");
    return u_map->size;
}

size_t u_map_capacity(const u_map_t* u_map) {
    HARD_ASSERT(u_map != nullptr, "u_map pointer is nullptr");
    return u_map->capacity;
}


#define HASH_MAP_PASS(u_map_, step_, start_idx_, index_, if_found_, if_not_found_)        \
    while(u_map_->slots[index_].state != EMPTY) {                                    \
            if(u_map_->slots[index_].state == USED &&                                \
            u_map_->key_cmp(u_map_->slots[index_].key, key)) {               \
                if_found_                                                                    \
                break;                                                                       \
            }                                                                                \
            index_ = (index_ + step_) % u_map_->capacity;                            \
            if(index_ == start_idx_) {if_not_found_ break;}                                  \
    }

bool u_map_get_elem(const u_map_t* u_map, const void* key, void* value_out) {
    HARD_ASSERT(u_map != nullptr, "u_map pointer is nullptr");
    HARD_ASSERT(key           != nullptr, "key pointer is nullptr");

    size_t step      = 0;
    size_t start_idx = get_index_and_step(u_map, key, &step);
    size_t index     = start_idx;
    
    HASH_MAP_PASS(u_map, step, start_idx, index, 
                  if(value_out != nullptr) 
                      memcpy(value_out, u_map->slots[index].value, u_map->value_size); 
                  return true;, 
    )

    return false;
}

error_t u_map_insert_elem(u_map_t* u_map, const void* key, const void* value) {
    HARD_ASSERT(u_map != nullptr, "u_map is nullptr");
    HARD_ASSERT(key           != nullptr, "Key is nulllptr");
    HARD_ASSERT(value         != nullptr, "Value is nullptr");

    LOGGER_DEBUG("Inserting elem started");

    size_t step      = 0;
    size_t start_idx = get_index_and_step(u_map, key, &step);
    size_t index     = start_idx;

    while(u_map->slots[index].state != EMPTY) {                                    
        index = (index + step) % u_map->capacity;   
        if(index == start_idx) return HM_ERR_FULL;                         
    }

    u_map->size++;
    error_t error = normalize_capacity(u_map);
    RETURN_IF_ERROR(error, u_map->size--;
                           u_map->slots[index].state = EMPTY);

    u_map->slots[index].state = USED;
    memcpy(u_map->slots[index].value, value, u_map->value_size);
    memcpy(u_map->slots[index].key,   key,   u_map->key_size);
    return HM_ERR_OK;
}

error_t u_map_remove_elem(u_map_t* u_map, const void* key, void* value_out) {
    HARD_ASSERT(u_map != nullptr, "u_map is nullptr");
    HARD_ASSERT(key           != nullptr, "Key is nulllptr");

    LOGGER_DEBUG("Removing elem started");

    unsigned char static_arr[MAX_TYPE_SIZE_BEFORE_CALLOC] = {};
    unsigned char* value = nullptr;
    if (u_map->value_size > MAX_TYPE_SIZE_BEFORE_CALLOC) {
        value = (unsigned char*)calloc(u_map->value_size, 1);
        if (!value) return HM_ERR_MEM_ALLOC;
    } else {
        value = static_arr;
    }

    size_t step      = 0;
    size_t start_idx = get_index_and_step(u_map, key, &step);
    size_t index     = start_idx;
    bool found = false;
    HASH_MAP_PASS(u_map, step, start_idx, index, 
                  if(value_out != nullptr) 
                      memcpy(value, u_map->slots[index].value, u_map->value_size); 
                  u_map->slots[index].state = DELETED;
                  found = true;, 
    )

    if (!found) {
        if (value != static_arr && value != nullptr)
            free(value);
        return HM_ERR_NOT_FOUND;
    }

    u_map->size--;
    error_t error = normalize_capacity(u_map);
    RETURN_IF_ERROR(error, u_map->size++;
                           u_map->slots[index].state = USED;
                           if(value_out != nullptr)
                               memcpy(u_map->slots[index].value, value, u_map->value_size););

    if(value_out != nullptr)
        memcpy(value_out, value, u_map->value_size);

    if (value != static_arr && value != nullptr)
        free(value);

    return HM_ERR_OK;
}

#undef HASH_MAP_PASS