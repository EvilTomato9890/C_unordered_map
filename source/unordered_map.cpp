#include "unordered_map.h"
#include "asserts.h"
#include "error_handler.h"
#include "logger.h"

#include <stdlib.h>
#include <stdio.h>
#include <memory.h>
#include <stdint.h>

const size_t INITIAL_CAPACITY = 32;
const double MAX_LOAD_FACTOR  = 0.7;
const double MIN_LOAD_FACTOR  = MAX_LOAD_FACTOR / 4.0;
const double REDUCTION_FACTOR = 2;
const double GROWTH_FACTOR    = 2;
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

static size_t round_up_to(size_t x, size_t align) {
    if (align <= 1) return x;
    size_t rem = x % align;
    return rem == 0 ? x : x + (align - rem);
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
    HARD_ASSERT(u_map           != nullptr, "u_map is nullptr");
    HARD_ASSERT(key             != nullptr, "Key is nullptr");
    HARD_ASSERT(step_out        != nullptr, "Step_out is nullptr");    
    HARD_ASSERT(u_map->capacity != 0, "Capacity is 0");

    size_t raw_hash = u_map->hash_func(key);
    size_t hash_1   = mix_hash(raw_hash);
    size_t hash_2   = mix_hash(raw_hash ^ (size_t)GOLD_64);

    size_t mod = u_map->capacity;
    if (mod > 1) *step_out = (hash_2 % (mod - 1)) | 1;
    else         *step_out = 1;
    return hash_1 % mod;

}

static error_t u_map_rehash(u_map_t* u_map, size_t new_capacity) {
    HARD_ASSERT(u_map != nullptr, "u_map is nullptr");

    if (u_map->is_static) return HM_ERR_OK;

    if (new_capacity < INITIAL_CAPACITY)
        new_capacity = INITIAL_CAPACITY;

    u_map_t new_map = {};
    error_t err = u_map_init(&new_map, new_capacity,
                             u_map->key_size,   u_map->key_align,
                             u_map->value_size, u_map->value_align,
                             u_map->hash_func,  u_map->key_cmp);
    RETURN_IF_ERROR(err);

    for (size_t i = 0; i < u_map->capacity; ++i) {
        if (u_map->slots[i].state != USED)
            continue;

        const void* key   = u_map->slots[i].key;
        const void* value = u_map->slots[i].value;

        size_t step = 0;
        size_t idx  = get_index_and_step(&new_map, key, &step);

        while (new_map.slots[idx].state == USED) {
            idx = (idx + step) % new_map.capacity;
        }

        memcpy(new_map.slots[idx].key,   key,   u_map->key_size);
        memcpy(new_map.slots[idx].value, value, u_map->value_size);
        new_map.slots[idx].state = USED;

        new_map.size++;
        new_map.occupied++;
    }

    free(u_map->slots);
    if (!u_map->is_static) {
        free(u_map->data);
    }

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
        new_capacity = (size_t)((double)u_map->capacity * GROWTH_FACTOR);
    }
    else if (u_map->capacity > INITIAL_CAPACITY && 
             load_real       < MIN_LOAD_FACTOR) {
        new_capacity = (size_t)((double)u_map->capacity / REDUCTION_FACTOR);
        if (new_capacity < INITIAL_CAPACITY)
            new_capacity = INITIAL_CAPACITY;
    }

    if (new_capacity == u_map->capacity)
        return HM_ERR_OK;

    LOGGER_DEBUG("Changing capacity from %zu, to %zu", u_map->capacity, new_capacity);
    return u_map_rehash(u_map, new_capacity);
}


//================================================================================
//                        Консрукторы, деконструкторы, копировальщики
//================================================================================

error_t u_map_init(u_map_t* u_map, size_t capacity, 
                   size_t key_size,      size_t key_align,
                   size_t value_size,    size_t value_align,
                   key_func_t hash_func, key_cmp_t key_cmp) {

    HARD_ASSERT(u_map       != nullptr, "u_map pointer is nullptr");
    HARD_ASSERT(hash_func   != nullptr, "hash_func pointer is nullptr");
    HARD_ASSERT(key_cmp     != nullptr, "key_cmp pointer is nullptr");
    HARD_ASSERT(key_size    > 0,        "key_size must be > 0");
    HARD_ASSERT(value_size  > 0,        "value_size must be > 0");
    HARD_ASSERT(key_align   > 0,        "key_align must be > 0");
    HARD_ASSERT(value_align > 0,        "value_align must be > 0");

    LOGGER_DEBUG("u_map_init started");

    if (capacity < INITIAL_CAPACITY)
        capacity = INITIAL_CAPACITY;
    else
        capacity = next_pow2_size_t(capacity);

    LOGGER_DEBUG("New capacity is: %zu", capacity);

    size_t keys_bytes       = capacity * key_size;
    size_t values_offset    = round_up_to(keys_bytes, value_align);
    size_t total_bytes      = values_offset + capacity * value_size;

    void* data = calloc(total_bytes, 1);
    if (!data) {
        return HM_ERR_MEM_ALLOC;
    }

    elem_t* slots = (elem_t*)calloc(capacity, sizeof(elem_t));
    if (!slots) {
        free(data);
        return HM_ERR_MEM_ALLOC;
    }

    void* base        = (char*)data;
    void* data_keys   = base;                   
    void* data_values = (char*)base + values_offset;    

    for (size_t i = 0; i < capacity; ++i) {
        slots[i].key   = (char*)data_keys   + i * key_size;
        slots[i].value = (char*)data_values + i * value_size;
        slots[i].state = EMPTY;
    }

    u_map->slots       = slots;
    u_map->data        = data;
    u_map->data_keys   = data_keys;
    u_map->data_values = data_values;

    u_map->size        = 0;
    u_map->occupied    = 0;
    u_map->capacity    = capacity;

    u_map->key_size    = key_size;
    u_map->key_align   = key_align;

    u_map->value_size  = value_size;
    u_map->value_align = value_align;

    u_map->hash_func   = hash_func;
    u_map->key_cmp     = key_cmp;

    u_map->is_static   = false;

    return HM_ERR_OK;
}

error_t u_map_static_init(u_map_t* u_map, void* data, size_t capacity, 
                          size_t key_size,      size_t key_align,
                          size_t value_size,    size_t value_align,
                          key_func_t hash_func, key_cmp_t key_cmp) {
    HARD_ASSERT(u_map       != nullptr, "u_map pointer is nullptr");
    HARD_ASSERT(data        != nullptr, "data pointer is nullptr");
    HARD_ASSERT(hash_func   != nullptr, "hash_func pointer is nullptr");
    HARD_ASSERT(key_cmp     != nullptr, "key_cmp pointer is nullptr");
    HARD_ASSERT(key_size    > 0,        "key_size must be > 0");
    HARD_ASSERT(value_size  > 0,        "value_size must be > 0");
    HARD_ASSERT(key_align   > 0,        "key_align must be > 0");
    HARD_ASSERT(value_align > 0,        "value_align must be > 0");
    
    LOGGER_DEBUG("u_map_static_init started");

    capacity = prev_pow2_size_t(capacity);
    RETURN_IF_ERROR(capacity == 0 ? HM_ERR_BAD_ARG : HM_ERR_OK);
    LOGGER_DEBUG("New capacity is: %zu", capacity);

    elem_t* slots = (elem_t*)calloc(capacity, sizeof(elem_t));
    RETURN_IF_ERROR(slots == nullptr ? HM_ERR_MEM_ALLOC : HM_ERR_OK);

    size_t keys_bytes    = capacity * key_size;
    size_t values_offset = round_up_to(keys_bytes, value_align ? value_align : 1);

    void* data_keys   = (char*)data;
    void* data_values = (char*)data + values_offset;

    for (size_t i = 0; i < capacity; i++) {
        slots[i].key   = (char*)data_keys   + i * key_size;
        slots[i].value = (char*)data_values + i * value_size;
        slots[i].state = EMPTY;
    }

    u_map->slots       = slots;
    u_map->data        = data;
    u_map->data_keys   = data_keys;
    u_map->data_values = data_values;

    u_map->size        = 0;
    u_map->occupied    = 0;
    u_map->capacity    = capacity;

    u_map->key_size    = key_size;
    u_map->key_align   = key_align;

    u_map->value_size  = value_size;
    u_map->value_align = value_align;

    u_map->hash_func   = hash_func;
    u_map->key_cmp     = key_cmp;

    u_map->is_static   = true;  

    return HM_ERR_OK;
}


error_t u_map_dest(u_map_t* u_map) {
    HARD_ASSERT(u_map != nullptr, "u_map pointer is nullptr");

    LOGGER_DEBUG("u_map_dest started");
    free(u_map->slots);
    if(!u_map->is_static) {
        free(u_map->data);
    }
    *u_map = {};
    return HM_ERR_OK;
}

error_t u_map_smart_copy(const u_map_t* source, u_map_t* target) {
    HARD_ASSERT(source != nullptr, "source u_map pointer is nullptr");
    HARD_ASSERT(target != nullptr, "target u_map pointer is nullptr");

    LOGGER_DEBUG("u_map_smart_copy started");

    error_t err = u_map_init(target, source->capacity,
                             source->key_size,   source->key_align,
                             source->value_size, source->value_align,
                             source->hash_func,  source->key_cmp);
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
        target->occupied++;
    }

    return HM_ERR_OK;
}

error_t u_map_raw_copy(const u_map_t* source, u_map_t* target) {
    HARD_ASSERT(source != nullptr, "source u_map pointer is nullptr");
    HARD_ASSERT(target != nullptr, "target u_map pointer is nullptr");

    LOGGER_DEBUG("u_map_raw_copy started");

    error_t err = u_map_init(target, source->capacity,
                             source->key_size,   source->key_align,
                             source->value_size, source->value_align,
                             source->hash_func,  source->key_cmp);
    RETURN_IF_ERROR(err);

    memcpy(target->data_keys,   source->data_keys,   source->capacity * source->key_size);
    memcpy(target->data_values, source->data_values, source->capacity * source->value_size);
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
    HARD_ASSERT(key   != nullptr, "key pointer is nullptr");

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
    HARD_ASSERT(key   != nullptr, "Key is nulllptr");
    HARD_ASSERT(value != nullptr, "Value is nullptr");

    LOGGER_DEBUG("Inserting elem started");

    error_t error = normalize_capacity(u_map);
    RETURN_IF_ERROR(error);

    size_t step      = 0;
    size_t start_idx = get_index_and_step(u_map, key, &step);
    size_t index     = start_idx;
    HASH_MAP_PASS(u_map, step, start_idx, index,  
                memcpy(u_map->slots[index].value, value, u_map->value_size);,
                return HM_ERR_FULL;);

    bool is_new =(u_map->slots[index].state != USED);

    u_map->slots[index].state = USED;
    memcpy(u_map->slots[index].value, value, u_map->value_size);
    memcpy(u_map->slots[index].key,   key,   u_map->key_size);
    if(is_new) {
        u_map->size++;
        u_map->occupied++;
    }

    return HM_ERR_OK;
}

error_t u_map_remove_elem(u_map_t* u_map, const void* key, void* value_out) {
    HARD_ASSERT(u_map != nullptr, "u_map is nullptr");
    HARD_ASSERT(key   != nullptr, "Key is nulllptr");

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
