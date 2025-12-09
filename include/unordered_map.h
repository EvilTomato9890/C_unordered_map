#ifndef UNORDERED_MAP_H_INCLUDED
#define UNORDERED_MAP_H_INCLUDED

#include <stdlib.h>

#include "error_handler.h"

//================================================================================

typedef size_t (*hash_func_t)(const void *key);
typedef bool   (*key_cmp_t  )(const void *a, const void *b);

enum elem_state_t {
    EMPTY,    
    USED,     
    DELETED   
};

struct elem_t {
    void*        value;
    void*        key; 
    elem_state_t state;
};

struct unordered_map_t {
    elem_t*     slots;
    void*       data;
    size_t      size;
    size_t      occupied;
    size_t      capacity;
    bool        is_capacity_pow2;
    size_t      value_size;
    size_t      key_size;
    hash_func_t hash_func;
    key_cmp_t   key_cmp;
    bool        is_static;
};
/*
struct fixed_unordered_map_t {
    const void*  keys;   
    const void*  values;
    const size_t size;
    const size_t capacity;
    hash_func_t  hash_func;
    key_cmp_t    key_cmp;
};
*/
//================================================================================
//                        Some defines
//================================================================================

#define COPY_ELEM(dest, src, unordered_map) \
    memcpy((dest), (src), (unordered_map)->key_size + (unordered_map)->value_size)

//================================================================================
//                        Консрукторы, деконструкторы, копировальщики
//================================================================================

error_t unordered_map_init(unordered_map_t* unordered_map, 
                           size_t capacity, size_t key_size, size_t value_size,
                           hash_func_t hash_func, key_cmp_t key_cmp);

error_t unordered_map_static_init(unordered_map_t* unordered_map, void* data, 
                                  size_t capacity, size_t key_size, size_t value_size,
                                  hash_func_t hash_func, key_cmp_t key_cmp);
//error_t init_fixed_unordered_map();

error_t unordered_map_dest(unordered_map_t* unordered_map);

error_t unordered_map_copy(const unordered_map_t* source, unordered_map_t* target);


//================================================================================
//                              Базовые функции
//================================================================================

size_t unordered_map_size    (const unordered_map_t* unordered_map);
size_t unordered_map_capacity(const unordered_map_t* unordered_map);
bool   unordered_map_is_empty(const unordered_map_t* unordered_map);

bool    get_elem   (const unordered_map_t* unordered_map, const void* key, void* value_out);
error_t insert_elem(unordered_map_t*       unordered_map, const void* key, const void* value);
error_t remove_elem(unordered_map_t*       unordered_map, const void* key, void* value_out);

//================================================================================
//                              Продвинутые функции
//================================================================================

error_t read_arr_to_unordered_map(unordered_map_t* unordered_map, const void* arr, size_t pair_count);

#endif