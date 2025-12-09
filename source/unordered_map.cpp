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

static size_t get_index_and_step(const unordered_map_t* unordered_map, const void* key, size_t* step_out) {
    HARD_ASSERT(unordered_map != nullptr, "Unordered_map is nullptr");
    HARD_ASSERT(key           != nullptr, "Key is nullptr");
    HARD_ASSERT(step_out      != nullptr, "Step_out is nullptr");    
    HARD_ASSERT(unordered_map->capacity != 0, "Capacity is 0");

    size_t raw_hash = unordered_map->hash_func(key) % unordered_map->capacity;
    size_t hash_1   = mix_hash(raw_hash);
    size_t hash_2   = mix_hash(raw_hash ^ (size_t)0x9e3779b97f4a7c15ULL);

    size_t mod = unordered_map->capacity;
    if (mod > 1) *step_out = (hash_2 % (mod - 1)) | 1;
    else         *step_out = 1;
    return hash_1 % mod;

}

static error_t normalize_capacity(unordered_map_t* unordered_map) {
    return HM_ERR_OK;
}

//================================================================================
//                        Консрукторы, деконструкторы, копировальщики
//================================================================================

error_t unordered_map_init(unordered_map_t* unordered_map, size_t capacity, 
                           size_t key_size, size_t value_size,
                           key_func_t hash_func, key_cmp_t key_cmp) {
    HARD_ASSERT(unordered_map != nullptr, "unordered_map pointer is nullptr");
    HARD_ASSERT(hash_func     != nullptr, "hash_func pointer is nullptr");
    HARD_ASSERT(key_cmp       != nullptr, "key_cmp pointer is nullptr");

    LOGGER_DEBUG("Unordered_map_init started");
    if(capacity < INITIAL_CAPACITY) capacity = INITIAL_CAPACITY;
    else                            capacity = next_pow2_size_t(capacity);
    LOGGER_DEBUG("New capacity is: %zu", capacity);

    unordered_map->is_capacity_pow2 = true;
    unordered_map->data       = calloc(capacity, key_size + value_size);
    RETURN_IF_ERROR(unordered_map->data == nullptr ? HM_ERR_MEM_ALLOC : HM_ERR_OK);
    unordered_map->slots      = (elem_t*)calloc(capacity, sizeof(elem_t));
    RETURN_IF_ERROR(unordered_map->slots == nullptr ? HM_ERR_MEM_ALLOC : HM_ERR_OK);

    for(size_t i = 0; i < capacity; i++) {
        unordered_map->slots[i].key   = (char*)unordered_map->data + i * (key_size + value_size);
        unordered_map->slots[i].value = (char*)unordered_map->data + i * (key_size + value_size) + key_size;
        unordered_map->slots[i].state = EMPTY;
    }

    unordered_map->size       = 0;
    unordered_map->capacity   = capacity;
    unordered_map->value_size = value_size;
    unordered_map->key_size   = key_size;
    unordered_map->hash_func  = hash_func;
    unordered_map->key_cmp    = key_cmp;
    unordered_map->is_static  = false;
    return HM_ERR_OK;
}

error_t unordered_map_static_init(unordered_map_t* unordered_map, void* data, 
                                  size_t capacity, size_t key_size, size_t value_size,
                                  key_func_t hash_func, key_cmp_t key_cmp) {
    HARD_ASSERT(unordered_map != nullptr, "unordered_map pointer is nullptr");
    HARD_ASSERT(hash_func     != nullptr, "hash_func pointer is nullptr");
    HARD_ASSERT(key_cmp       != nullptr, "key_cmp pointer is nullptr");

    LOGGER_DEBUG("Unordered_map_static_init started");
    if(capacity < INITIAL_CAPACITY) capacity = INITIAL_CAPACITY;
    LOGGER_DEBUG("New capacity is: %zu", capacity);

    unordered_map->slots = (elem_t*)calloc(capacity, sizeof(elem_t));
    unordered_map->is_capacity_pow2 = is_pow2(capacity);
    RETURN_IF_ERROR(unordered_map->slots == nullptr ? HM_ERR_MEM_ALLOC : HM_ERR_OK);

    unordered_map->data = data;
    for(size_t i = 0; i < capacity; i++) {
        unordered_map->slots[i].key   = (char*)unordered_map->data + i * (key_size + value_size);
        unordered_map->slots[i].value = (char*)unordered_map->data + i * (key_size + value_size) + key_size;
        unordered_map->slots[i].state = EMPTY;
    }

    unordered_map->size       = 0;
    unordered_map->capacity   = capacity;
    unordered_map->value_size = value_size;
    unordered_map->key_size   = key_size;
    unordered_map->hash_func  = hash_func;
    unordered_map->key_cmp    = key_cmp;
    unordered_map->is_static  = true;
    return HM_ERR_OK;
}

error_t unordered_map_dest(unordered_map_t* unordered_map) {
    HARD_ASSERT(unordered_map != nullptr, "unordered_map pointer is nullptr");

    LOGGER_DEBUG("Unordered_map_dest started");
    free(unordered_map->slots);
    if(!unordered_map->is_static) {
        free(unordered_map->data);
    }
    unordered_map->slots = nullptr;
    unordered_map->data  = nullptr;
    unordered_map->size  = 0;
    unordered_map->capacity = 0;
    return HM_ERR_OK;
}

error_t unordered_map_copy(const unordered_map_t* source, unordered_map_t* target) {
    HARD_ASSERT(source != nullptr, "source unordered_map pointer is nullptr");
    HARD_ASSERT(target != nullptr, "target unordered_map pointer is nullptr");

    LOGGER_DEBUG("Unordered_map_copy started");

    error_t err = unordered_map_init(target, source->capacity, source->key_size, 
                                     source->value_size, source->hash_func, source->key_cmp);
    RETURN_IF_ERROR(err);

    for(size_t i = 0; i < source->capacity; i++) {
        if(source->slots[i].state == USED) {
            size_t hash = source->hash_func(source->slots[i].key) % target->capacity;
            while(target->slots[hash].state == USED) {
                hash = (hash + 1) % target->capacity;
            }
            COPY_ELEM(target->slots[hash].key,   source->slots[i].key,   source);
            COPY_ELEM(target->slots[hash].value, source->slots[i].value, source);
            target->slots[hash].state = USED;
            target->size++;
        }
    }

    return HM_ERR_OK;
}


//================================================================================
//                              Базовые функции
//================================================================================

bool unordered_map_is_empty(const unordered_map_t* unordered_map) {
    HARD_ASSERT(unordered_map != nullptr, "unordered_map pointer is nullptr");
    return unordered_map->size == 0;
}

size_t unordered_map_size    (const unordered_map_t* unordered_map) {
    HARD_ASSERT(unordered_map != nullptr, "unordered_map pointer is nullptr");
    return unordered_map->size;
}

size_t unordered_map_capacity(const unordered_map_t* unordered_map) {
    HARD_ASSERT(unordered_map != nullptr, "unordered_map pointer is nullptr");
    return unordered_map->capacity;
}


#define HASH_MAP_PASS(unordered_map_, step_, start_idx_, index_, if_found_, if_not_found_)        \
    while(unordered_map_->slots[index_].state != EMPTY) {                                    \
            if(unordered_map_->slots[index_].state == USED &&                                \
            unordered_map_->key_cmp(unordered_map_->slots[index_].key, key)) {               \
                if_found_                                                                    \
                break;                                                                       \
            }                                                                                \
            index_ = (index_ + step_) % unordered_map_->capacity;                            \
            if(index_ == start_idx_) {if_not_found_ break;}                                  \
    }

bool get_elem(const unordered_map_t* unordered_map, const void* key, void* value_out) {
    HARD_ASSERT(unordered_map != nullptr, "unordered_map pointer is nullptr");
    HARD_ASSERT(key           != nullptr, "key pointer is nullptr");

    size_t step      = 0;
    size_t start_idx = get_index_and_step(unordered_map, key, &step);
    size_t index     = start_idx;
    
    HASH_MAP_PASS(unordered_map, step, start_idx, index, 
                  if(value_out != nullptr) 
                      COPY_ELEM(value_out, unordered_map->slots[index].value, unordered_map); 
                  return true;, 
    )

    return false;
}

error_t insert_elem(unordered_map_t* unordered_map, const void* key, const void* value) {
    HARD_ASSERT(unordered_map != nullptr, "Unordered_map is nullptr");
    HARD_ASSERT(key           != nullptr, "Key is nulllptr");
    HARD_ASSERT(value         != nullptr, "Value is nullptr");

    LOGGER_DEBUG("Inserting elem started");

    size_t step      = 0;
    size_t start_idx = get_index_and_step(unordered_map, key, &step);
    size_t index     = start_idx;

    while(unordered_map->slots[index].state != EMPTY) {                                    
        index = (index + step) % unordered_map->capacity;   
        if(index == start_idx) return HM_ERR_FULL;                         
    }

    unordered_map->size++;
    error_t error = normalize_capacity(unordered_map);
    RETURN_IF_ERROR(error, unordered_map->size--;
                           unordered_map->slots[index].state = EMPTY);

    unordered_map->slots[index].state = USED;
    COPY_ELEM(unordered_map->slots[index].value, value, unordered_map);
    COPY_ELEM(unordered_map->slots[index].key,   key,   unordered_map);
    return HM_ERR_OK;
}

error_t remove_elem(unordered_map_t* unordered_map, const void* key, void* value_out) {
    HARD_ASSERT(unordered_map != nullptr, "Unordered_map is nullptr");
    HARD_ASSERT(key           != nullptr, "Key is nulllptr");

    void*  value     = 0;
    size_t step      = 0;
    size_t start_idx = get_index_and_step(unordered_map, key, &step);
    size_t index     = start_idx;

    HASH_MAP_PASS(unordered_map, step, start_idx, index, 
                  if(value_out != nullptr) 
                      COPY_ELEM(value, unordered_map->slots[index].value, unordered_map); 
                  unordered_map->slots[index].state = DELETED;, 

                  return HM_ERR_NOT_FOUND;
    )

    unordered_map->size--;
    error_t error = normalize_capacity(unordered_map);
    RETURN_IF_ERROR(error, unordered_map->size++;
                           unordered_map->slots[index].state = USED);
    
    COPY_ELEM(value_out, value, unordered_map);
    return HM_ERR_OK;
}

#undef HASH_MAP_PASS