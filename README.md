# u_map — небольшая хэш-таблица на C

Мини-библиотека открытой адресации с двойным хэшированием. Поддерживает как обычный динамический режим, так и «статический» режим поверх заранее выделенного буфера.

---

## Структура проекта

- `unordered_map.h`   — публичный API, типы и макросы инициализации.  
- `unordered_map.cpp` — реализация хэш-таблицы.  
- `Makefile.lib`      — сборка статической библиотеки (см. цели внутри файла).

Зависимости (подключаются через `#include`):

- `asserts.h`       — макросы `HARD_ASSERT(...)`
- `error_handler.h` — тип `error_t`, коды ошибок `HM_ERR_...`
- `logger.h`        — `LOGGER_DEBUG`, `LOGGER_WARNING` и т.п.

---

## Основные возможности

- Открытая адресация с **двойным хэшированием** (`hash_1`, `hash_2`).  
- Специальный миксер `mix_hash` на основе SplitMix64 для равномерного распределения.  
- Ёмкость — степень двойки, автоматическое:
  - **расширение** при загрузке > `0.7`
  - **сжатие** при загрузке < `0.7 / 4` (но не ниже минимальной ёмкости).  
- Различение состояний ячейки: `EMPTY`, `USED`, `DELETED`.  
- Два режима работы:
  - **Динамический** (`u_map_init`) — память внутри карты, `realloc/rehash` разрешён.
  - **Статический** (`u_map_static_init`) — ключи/значения хранятся в внешнем заранее выделенном буфере, `realloc` **не выполняется**.  
- Копирование:
  - «Умное» копирование с перехэшированием (`u_map_smart_copy`).
  - «Сырой» копирующий клон (`u_map_raw_copy`).  

---

## API (кратко)

### Типы

```c
typedef size_t (*key_func_t)(const void *key);
typedef bool   (*key_cmp_t )(const void *a, const void *b);

struct u_map_t {
    // внутренние поля, см. header
};
```

Состояние ячейки:

```c
enum elem_state_t {
    EMPTY,
    USED,
    DELETED
};
```

### Упрощённая инициализация

```c
#define SIMPLE_U_MAP_INIT(u_map_, capacity_, key_type_, value_type_, hash_func_, key_cmp_)
#define SIMPLE_U_MAP_STATIC_INIT(u_map_, data_, capacity_, key_type_, value_type_, hash_func_, key_cmp_)
```

### Конструкторы / деструктор

```c
error_t u_map_init(
    u_map_t* u_map, size_t capacity,
    size_t key_size,      size_t key_align,
    size_t value_size,    size_t value_align,
    key_func_t hash_func, key_cmp_t key_cmp
);

error_t u_map_static_init(
    u_map_t* u_map, void* data, size_t capacity,
    size_t key_size,      size_t key_align,
    size_t value_size,    size_t value_align,
    key_func_t hash_func, key_cmp_t key_cmp
);

error_t u_map_dest(u_map_t* u_map);
```

### Базовые операции

```c
size_t u_map_size    (const u_map_t* u_map);
size_t u_map_capacity(const u_map_t* u_map);
bool   u_map_is_empty(const u_map_t* u_map);

bool    u_map_get_elem   (const u_map_t* u_map, const void* key, const void* value_out);
error_t u_map_insert_elem(u_map_t*       u_map, const void* key, const void* value);
error_t u_map_remove_elem(u_map_t*       u_map, const void* key,       void* value_out);
```

---

## Сборка

### Статическая библиотека

В проекте есть `Makefile.lib`. Типичный сценарий:

```bash
# Сборка статической библиотеки
make -f Makefile.lib

# Очистка
make -f Makefile.lib clean
```

Имя итоговой библиотеки и точные цели см. внутри `Makefile.lib`.

### Логирование

Код опирается на `logger.h` и может использовать compile-time флаг (например, `HASH_LOGGER_ALL`), чтобы включать/отключать детальный лог хэш-карты при сборке:
Для этого существует дополнительный режим запуска - logger
```bash
# Пример: включить расширенный лог
make -f Makefile.lib logger
```



---

## Пример использования

Простейшая карта `int -> int`:

```c
#include "unordered_map.h"
#include <stdio.h>
#include <stdint.h>

static size_t int_hash(const void* ptr) {
    int key = *(const int*)ptr;
    // простой, но допустимый хэш — библиотека всё равно перемешает mix_hash
    return (size_t)key;
}

static bool int_equal(const void* a, const void* b) {
    return *(const int*)a == *(const int*)b;
}

int main(void) {
    u_map_t map = {};

    if (SIMPLE_U_MAP_INIT(&map, 32, int, int, int_hash, int_equal) != HM_ERR_OK) {
        fprintf(stderr, "u_map_init failed
");
        return 1;
    }

    int key   = 42;
    int value = 1337;

    if (u_map_insert_elem(&map, &key, &value) != HM_ERR_OK) {
        fprintf(stderr, "insert failed
");
        u_map_dest(&map);
        return 1;
    }

    int out = 0;
    if (u_map_get_elem(&map, &key, &out)) {
        printf("Found: key=%d value=%d\n", key, out);
    } else {
        printf("Key not found\n");
    }

    u_map_dest(&map);
    return 0;
}
```

---

## Особенности реализации
- **64-битный `size_t`**: Если size_t другого размера работа может быть несколько хуже. В `mix_hash` есть проверка, и при отличном размере выводится предупреждение логгером.  
- Ёмкость всегда — степень двойки (`next_pow2_size_t` / `prev_pow2_size_t`), что упрощает расчёт индексов и шага.  
- Вставка:
  - При существующем ключе значение **перезаписывается**, `size` не растёт.
  - При новом ключе увеличиваются `size` и `occupied` и может произойти `normalize_capacity` (увеличение/уменьшение ёмкости).  
- Удаление:
  - Ячейка помечается `DELETED`, размер уменьшается.
  - После этого может произойти `normalize_capacity` (увеличение/уменьшение ёмкости).  
- Статический режим (`is_static = true`) **запрещает реаллокацию**: `u_map_rehash` просто возвращает `HM_ERR_OK`, не меняя ёмкость.  

---

## Ограничения

- Потокобезопасность отсутствует: структура **не рассчитана на многопоточную запись/чтение** без внешней синхронизации.
- Для статического режима пользователь обязан:
  - Выделить буфер нужного размера.
  - Корректно задать `capacity`, `key_size`, `value_size`, выравнивание.
- Библиотека полагается на `error_handler.h`, `asserts.h`, `logger.h` — без них проект не соберётся.

---

## Идеи для доработки

- Итераторы по элементам.
- Пользовательские коллбеки для уничтожения ключей/значений (RAII-подобное поведение).
- Отдельный API для `read_arr_to_u_map` (объявлен в header, может быть реализован как bulk-загрузка).  
