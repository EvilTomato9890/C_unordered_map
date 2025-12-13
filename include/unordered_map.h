#ifndef U_MAP_H_INCLUDED
#define U_MAP_H_INCLUDED

#include <stdlib.h>
#include <stdbool.h>
#include <stdalign.h>
#include <stddef.h>

#include "error_handler.h"

//================================================================================

typedef size_t (*key_func_t)(const void *key);
typedef bool   (*key_cmp_t )(const void *a, const void *b);

typedef enum elem_state_t {
    EMPTY   = 0,
    USED    = 1,
    DELETED = 2,
} elem_state_t;

typedef struct u_map_t {
    void*         data;         
    void*         data_keys;    
    void*         data_values;  
    elem_state_t* data_states;   

    size_t        size;        
    size_t        occupied;     
    size_t        capacity;     

    size_t        key_size;
    size_t        key_align;
    size_t        key_stride;   

    size_t        value_size;
    size_t        value_align;
    size_t        value_stride; 

    key_func_t    hash_func;
    key_cmp_t     key_cmp;

    bool          is_static;
} u_map_t;

//================================================================================
//                      Функции-помощники
//================================================================================

// How many bytes are required for a static buffer (capacity is rounded DOWN to pow2 internally).
size_t u_map_required_bytes(size_t capacity,
                            size_t key_size,   size_t key_align,
                            size_t value_size, size_t value_align);


//================================================================================
//                       Конструкторы / Деконструкторы /Копировальщиеи
//================================================================================

// - capacity for dynamic init is rounded UP to pow2 (and >= INITIAL_CAPACITY inside .cpp).
// - key/value alignment is important if your hash/compare dereference typed pointers.

error_t u_map_init(u_map_t* u_map, size_t capacity,
                   size_t key_size,   size_t key_align,
                   size_t value_size, size_t value_align,
                   key_func_t hash_func, key_cmp_t key_cmp);


// - capacity is rounded DOWN to pow2 (must be > 0).
// - caller must provide a buffer aligned at least to max(key_align, value_align, alignof(elem_state_t))
// - buffer must be at least u_map_required_bytes(capacity, ...)
error_t u_map_static_init(u_map_t* u_map, void* data, size_t capacity,
                          size_t key_size,   size_t key_align,
                          size_t value_size, size_t value_align,
                          key_func_t hash_func, key_cmp_t key_cmp);

error_t u_map_dest(u_map_t* u_map);

error_t u_map_smart_copy(u_map_t* target, const u_map_t* source);
error_t u_map_raw_copy  (u_map_t* target, const u_map_t* source);


//================================================================================
//                              Базовые функции
//================================================================================

size_t u_map_size    (const u_map_t* u_map);
size_t u_map_capacity(const u_map_t* u_map);
bool   u_map_is_empty(const u_map_t* u_map);

bool    u_map_get_elem   (const u_map_t* u_map, const void* key, void* value_out);
error_t u_map_insert_elem(u_map_t*       u_map, const void* key, const void* value);
error_t u_map_remove_elem(u_map_t*       u_map, const void* key, void* value_out);


//================================================================================
//                              Продвинутые функции
//================================================================================

error_t read_arr_to_u_map(u_map_t* u_map, const void* arr, size_t pair_count);


//================================================================================
//                        Макросы-обертки
//================================================================================

#define SIMPLE_U_MAP_INIT(u_map_, capacity_, key_type_, value_type_, hash_func_, key_cmp_) \
    u_map_init((u_map_), (capacity_),                                                     \
               sizeof(key_type_),   alignof(key_type_),                                   \
               sizeof(value_type_), alignof(value_type_),                                 \
               (hash_func_), (key_cmp_))

#define SIMPLE_U_MAP_STATIC_INIT(u_map_, data_, capacity_, key_type_, value_type_, hash_func_, key_cmp_) \
    u_map_static_init((u_map_), (data_), (capacity_),                                                     \
                      sizeof(key_type_),   alignof(key_type_),                                             \
                      sizeof(value_type_), alignof(value_type_),                                           \
                      (hash_func_), (key_cmp_))

#endif 