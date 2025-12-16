# Unordered Map (u_map) — хэш‑таблица на C

Ключи и значения хранятся как **сырые байты** фиксированного размера, сравнение и хэширование задаются
пользователем через коллбеки. Публичный интерфейс — один заголовок `unordered_map.h`.

## Особенности

- **Open addressing** + **double hashing** (пробирование по шагу).
- Ёмкость всегда **степень двойки** (внутри корректируется).
- Поддержка:
  - **динамической** таблицы (память внутри модуля, `u_map_init`)
  - **статической** таблицы (память задаёшь сам, `u_map_static_init`)

## Быстрый старт

### 1) Подключение

Внешнему коду нужен только:
- `unordered_map.h`

Внутри реализации используются и должны быть доступны при сборке `.cpp`:
- `asserts.h`, `logger.h`, `error_handler.h`

### 2) Пример использования (int → double)

```c
#include "unordered_map.h"
#include <stdint.h>
#include <string.h>

static size_t hash_int(const void* p) {
    uint32_t x = *(const uint32_t*)p;
    return (size_t)x;
}

static bool cmp_int(const void* a, const void* b) {
    return *(const uint32_t*)a == *(const uint32_t*)b;
}

int main() {
    u_map_t m = {};
    if (u_map_init(&m, 64,
                   sizeof(uint32_t), alignof(uint32_t),
                   sizeof(double),   alignof(double),
                   hash_int, cmp_int) != HM_ERR_OK) {
        return 1;
    }

    uint32_t k = 10;
    double v = 3.14;
    u_map_insert_elem(&m, &k, &v);

    double out = 0;
    if (u_map_get_elem(&m, &k, &out)) {
        // out == 3.14
    }

    u_map_destroy(&m);
    return 0;
}
```

## API

### Типы

- `key_func_t`: `size_t (*)(const void* key)` — хэш ключа
- `key_cmp_t`: `bool (*)(const void* a, const void* b)` — сравнение ключей
- `u_map_t` — структура таблицы (поля считаем внутренними).

### Конструкторы / деструкторы / копировальщики

- `error_t u_map_init(...)`  
  Динамическая таблица. `capacity` gjdsiftncz до степени двойки и, как минимум, до внутреннего `INITIAL_CAPACITY`.

- `error_t u_map_static_init(...)`  
  Таблица в переданном буфере:
  - `capacity` округляется **вниз** до степени двойки, и должна быть `> 0`
  - буфер обязан быть выровнен хотя бы по `max(key_align, value_align, alignof(elem_state_t))`
  - размер буфера: `u_map_required_bytes(capacity, ...)`

- `error_t u_map_destroy(u_map_t* u_map)`  
  Освобождает память **только** для динамической таблицы; для статической — просто обнуляет структуру.

- `error_t u_map_smart_copy(u_map_t* target, const u_map_t* source)`  
  Копирует только `USED` элементы (через вставку в новый map).

- `error_t u_map_raw_copy(u_map_t* target, const u_map_t* source)`  
  Копирует весь внутренний буфер “как есть”.

### Базовые функции

- `bool u_map_get_elem(const u_map_t* u_map, const void* key, void* value_out)`  
  Возвращает `true/false`. Если `value_out != NULL`, копирует значение.

- `error_t u_map_insert_elem(u_map_t* u_map, const void* key, const void* value)`  
  - если при вызове требуется может сделать rehash/resize
  - если ключ уже есть — **обновляет значение**
  - если ключ новый — вставляет

- `error_t u_map_remove_elem(u_map_t* u_map, const void* key, void* value_out)`  
  - если при вызове требуется может сделать rehash/resize
  - если ключ найден — помечает слот `DELETED`, уменьшает `size`
  - если `value_out != NULL` — возвращает удалённое значение.

- `size_t u_map_size(const u_map_t* u_map)` / `u_map_capacity(...)` / `u_map_is_empty(...)`

### Продвинутые функции

- `error_t read_arr_to_u_map(u_map_t* u_map, const void* arr, size_t pair_count)`  
  Читает пары `{key, value}` из массива фиксированного формата и вставляет их в map.

### Макросы‑обёртки

- `SIMPLE_U_MAP_INIT(...)`
- `SIMPLE_U_MAP_STATIC_INIT(...)`

## Как работает resize / rehash (кратко)

Внутри поддерживаются две “загрузки”:
- **real load** = `size / capacity` (только `USED`)
- **occupied load** = `occupied / capacity` (`USED + DELETED`)

Типичная логика:
- если `real load` слишком маленький — таблица может **сжаться**
- если `occupied load` слишком большой:
  - если garbage_load слишком большой делается **rehash на той же capacity**
  - иначе — **рост capacity в 2 раза**

## Сборка (пример)

```bash
g++ -c unordered_map.cpp -O2 -std=gnu++17
ar rcs libumap.a unordered_map.o
```

Подключение библиотеки:
```bash
g++ main.cpp -L. -lumap
```
