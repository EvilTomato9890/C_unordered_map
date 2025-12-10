#ifndef u_map_H_INCLUDED
#define u_map_H_INCLUDED

#include <stdlib.h>

#include "error_handler.h"

//================================================================================

typedef size_t (*key_func_t)(const void *key);
typedef bool   (*key_cmp_t )(const void *a, const void *b);

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

struct u_map_t {
    elem_t*     slots;
    void*       data_keys;
    void*       data_values;
    void*       data;

    size_t      size;
    size_t      occupied;
    size_t      capacity;

    size_t      value_size;
    size_t      value_align;

    size_t      key_size;
    size_t      key_align;

    key_func_t  hash_func;
    key_cmp_t   key_cmp;

    bool        is_static;
};

/*
struct fixed_u_map_t {
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


//================================================================================
//                        Консрукторы, деконструкторы, копировальщики
//================================================================================
/*Использование: SIMPLE_U_MAP_INIT(указатель на структуру u_map_t, 
                                   желаемый размер (Будет увеличен до ближайшей степени 2-ки),
                                   тип ключа,
                                   тип значения,
                                   ваша хэш функция для ключа (Можно не беспокоиться о неравномерности распределения),
                                   ваша функция сравнения

*/
#define SIMPLE_U_MAP_INIT(u_map_, capacity_, key_type_, value_type_, hash_func_, key_cmp_) \
    u_map_init((u_map_), (capacity_),                     \
                sizeof(key_type_),   alignof(key_type_),  \
                sizeof(value_type_), alignof(key_type_),  \
                (hash_func_),          (key_cmp_));

error_t u_map_init(u_map_t* u_map, size_t capacity, 
                   size_t key_size,      size_t key_align,
                   size_t value_size,    size_t value_align,
                   key_func_t hash_func, key_cmp_t key_cmp);


#define SIMPLE_U_MAP_STATIC_INIT(u_map_, data_, capacity_, key_type_, value_type_, hash_func_, key_cmp_)                        \
    u_map_static_init((u_map_), (data_), (capacity_),     \
                sizeof(key_type_),   alignof(key_type_),  \
                sizeof(value_type_), alignof(key_type_),  \
                (hash_func_),          (key_cmp_));

error_t u_map_static_init(u_map_t* u_map, void* data, size_t capacity, 
                          size_t key_size,      size_t key_align,
                          size_t value_size,    size_t value_align,
                          key_func_t hash_func, key_cmp_t key_cmp);
//error_t init_fixed_u_map();

error_t u_map_dest(u_map_t* u_map);

error_t u_map_smart_copy(const u_map_t* source, u_map_t* target);

error_t u_map_raw_copy(const u_map_t* source, u_map_t* target);


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

#endif