// u_map_test.cpp
#include "unordered_map.h"
#include "logger.h"
#include "error_handler.h"

#include <cstdio>
#include <cstring>
#include <cstddef>   // для alignof
#include <cassert>

static size_t size_t_hash(const void* a) {
    size_t v;
    memcpy(&v, a, sizeof(v));   // безопасно даже при странном выравнивании
    return v;
}

static bool size_t_eq(const void* a, const void* b) {
    size_t x, y;
    memcpy(&x, a, sizeof(x));
    memcpy(&y, b, sizeof(y));
    return x == y;
}

static int g_failed = 0;

#define CHECK(cond, msg)                                              \
    do {                                                              \
        if (cond) {                                                   \
            std::printf("[ OK ] %s\n", msg);                          \
        } else {                                                      \
            std::printf("[FAIL] %s\n", msg);                          \
            g_failed++;                                               \
        }                                                             \
    } while (0)

//---------------------------------------------------------------
// Тест 1: базовые операции insert / get / remove
//---------------------------------------------------------------
static void test_basic() {
    std::puts("=== test_basic ===");

    u_map_t m = {};
    error_t err = u_map_init(&m,
                             4,                                // capacity hint
                             sizeof(size_t), alignof(size_t), // key
                             sizeof(int),    alignof(int),    // value
                             size_t_hash,
                             size_t_eq);
    CHECK(err == HM_ERR_OK, "u_map_init returns OK");

    size_t k1 = 42;
    int    v1 = 100;
    err = u_map_insert_elem(&m, &k1, &v1);
    CHECK(err == HM_ERR_OK, "insert k1");
    CHECK(u_map_size(&m) == 1, "size == 1 after first insert");

    int out = 0;
    bool found = u_map_get_elem(&m, &k1, &out);
    CHECK(found,          "get existing key k1");
    CHECK(out == v1,      "value for k1 is correct");

    // перезапись значения по тому же ключу
    int v2 = 777;
    err = u_map_insert_elem(&m, &k1, &v2);
    CHECK(err == HM_ERR_OK, "re-insert same key k1");
    // ожидаем size не увеличился (если реализуешь именно так)
    CHECK(u_map_size(&m) == 1, "size still == 1 after reinsert same key (ожидаемое поведение)");

    out = 0;
    found = u_map_get_elem(&m, &k1, &out);
    CHECK(found,          "get k1 after reinsert");
    CHECK(out == v2,      "value for k1 updated");

    // удаление
    int removed = 0;
    err = u_map_remove_elem(&m, &k1, &removed);
    CHECK(err == HM_ERR_OK, "remove k1 returns OK");
    CHECK(removed == v2,    "removed value matches last stored");
    CHECK(u_map_size(&m) == 0, "size == 0 after remove");

    out = 0;
    found = u_map_get_elem(&m, &k1, &out);
    CHECK(!found, "get removed key returns false");

    u_map_dest(&m);
}

//---------------------------------------------------------------
// Тест 2: рост capacity (rehash вверх)
//---------------------------------------------------------------
static void test_growth() {
    std::puts("=== test_growth ===");

    u_map_t m = {};
    error_t err = u_map_init(&m,
                             2,
                             sizeof(size_t), alignof(size_t),
                             sizeof(int),    alignof(int),
                             size_t_hash,
                             size_t_eq);
    CHECK(err == HM_ERR_OK, "u_map_init for growth test");

    size_t initial_cap = u_map_capacity(&m);

    // Вставляем много элементов, чтобы переполнить load_factor
    const size_t N = 200;
    for (size_t i = 0; i < N; ++i) {
        int v = (int)(i * 10);
        err = u_map_insert_elem(&m, &i, &v);
        CHECK(err == HM_ERR_OK, "insert many elements (growth)");
    }

    size_t final_cap = u_map_capacity(&m);
    CHECK(final_cap >= initial_cap, "capacity did not shrink after many inserts");
    CHECK(u_map_size(&m) == N,      "size == N after many inserts");

    // Проверим несколько значений
    for (size_t i = 0; i < N; i += 37) {
        int out = 0;
        bool found = u_map_get_elem(&m, &i, &out);
        CHECK(found, "get existing key after rehash");
        CHECK(out == (int)(i * 10), "value correct after rehash");
    }

    u_map_dest(&m);
}

//---------------------------------------------------------------
// Тест 3: shrink (rehash вниз) + tombstone
//---------------------------------------------------------------
static void test_shrink_and_tombstones() {
    std::puts("=== test_shrink_and_tombstones ===");

    u_map_t m = {};
    error_t err = u_map_init(&m,
                             32,
                             sizeof(size_t), alignof(size_t),
                             sizeof(int),    alignof(int),
                             size_t_hash,
                             size_t_eq);
    CHECK(err == HM_ERR_OK, "u_map_init for shrink test");

    const size_t N = 64;
    for (size_t i = 0; i < N; ++i) {
        int v = (int)i;
        err = u_map_insert_elem(&m, &i, &v);
        CHECK(err == HM_ERR_OK, "insert for shrink test");
    }

    size_t cap_before = u_map_capacity(&m);

    // удаляем почти всё, чтобы заставить таблицу ужаться
    for (size_t i = 0; i < N - 2; ++i) {
        int removed = -1;
        err = u_map_remove_elem(&m, &i, &removed);
        CHECK(err == HM_ERR_OK, "remove existing (shrink)");
        CHECK(removed == (int)i, "removed value correct");
    }

    size_t cap_after = u_map_capacity(&m);
    CHECK(cap_after <= cap_before, "capacity shrank or stayed same");

    // Проверка, что оставшиеся ключи живы
    for (size_t i = N - 2; i < N; ++i) {
        int out = -1;
        bool found = u_map_get_elem(&m, &i, &out);
        CHECK(found, "remaining keys exist after shrink");
        CHECK(out == (int)i, "remaining values correct");
    }

    u_map_dest(&m);
}

//---------------------------------------------------------------
// Тест 4: raw_copy / smart_copy
//---------------------------------------------------------------
static void test_copy() {
    std::puts("=== test_copy ===");

    u_map_t src = {};
    error_t err = u_map_init(&src,
                             8,
                             sizeof(size_t), alignof(size_t),
                             sizeof(int),    alignof(int),
                             size_t_hash,
                             size_t_eq);
    CHECK(err == HM_ERR_OK, "u_map_init for copy test");

    for (size_t i = 0; i < 10; ++i) {
        int v = (int)(i * 3);
        err = u_map_insert_elem(&src, &i, &v);
        CHECK(err == HM_ERR_OK, "insert into src");
    }

    // RAW COPY
    u_map_t dst_raw = {};
    err = u_map_raw_copy(&src, &dst_raw);
    CHECK(err == HM_ERR_OK, "u_map_raw_copy returns OK");
    CHECK(u_map_size(&dst_raw) == u_map_size(&src), "raw_copy keeps size");

    for (size_t i = 0; i < 10; ++i) {
        int out1 = -1, out2 = -1;
        bool f1 = u_map_get_elem(&src,    &i, &out1);
        bool f2 = u_map_get_elem(&dst_raw,&i, &out2);
        CHECK(f1 && f2, "raw_copy: both maps have key");
        CHECK(out1 == out2, "raw_copy: values are equal");
    }

    // SMART COPY
    u_map_t dst_smart = {};
    err = u_map_smart_copy(&src, &dst_smart);
    CHECK(err == HM_ERR_OK, "u_map_smart_copy returns OK");
    CHECK(u_map_size(&dst_smart) == u_map_size(&src), "smart_copy keeps size");

    for (size_t i = 0; i < 10; ++i) {
        int out1 = -1, out2 = -1;
        bool f1 = u_map_get_elem(&src,      &i, &out1);
        bool f2 = u_map_get_elem(&dst_smart,&i, &out2);
        CHECK(f1 && f2, "smart_copy: both maps have key");
        CHECK(out1 == out2, "smart_copy: values are equal");
    }

    u_map_dest(&src);
    u_map_dest(&dst_raw);
    u_map_dest(&dst_smart);
}

//---------------------------------------------------------------
// Тест 5: static init
//---------------------------------------------------------------
static void test_static_init() {
    std::puts("=== test_static_init ===");

    const size_t cap        = 8;
    const size_t key_size   = sizeof(size_t);
    const size_t key_align  = alignof(size_t);
    const size_t val_size   = sizeof(int);
    const size_t val_align  = alignof(int);

    // Посчитаем размер буфера так же, как делает u_map_static_init
    size_t keys_bytes    = cap * key_size;
    size_t values_offset = keys_bytes;
    if (val_align > 1) {
        size_t rem = values_offset % val_align;
        if (rem) values_offset += (val_align - rem);
    }
    size_t total_bytes = values_offset + cap * val_size;

    void* buffer = std::calloc(1, total_bytes);
    CHECK(buffer != nullptr, "buffer for static init allocated");

    u_map_t m = {};
    error_t err = u_map_static_init(&m,
                                    buffer,
                                    cap,
                                    key_size,  key_align,
                                    val_size,  val_align,
                                    size_t_hash,
                                    size_t_eq);
    CHECK(err == HM_ERR_OK, "u_map_static_init returns OK");

    // Простейшие операции
    size_t k = 123;
    int    v = 999;
    err = u_map_insert_elem(&m, &k, &v);
    CHECK(err == HM_ERR_OK, "insert into static map");

    int out = 0;
    bool found = u_map_get_elem(&m, &k, &out);
    CHECK(found, "get from static map");
    CHECK(out == v, "value from static map correct");

    // dest не освобождает data при is_static = true
    u_map_dest(&m);
    std::free(buffer);
}

//---------------------------------------------------------------

int main() {
    logger_initialize_stream(stderr);

    test_basic();
    test_growth();
    test_shrink_and_tombstones();
    test_copy();
    test_static_init();

    if (g_failed == 0) {
        std::puts("=== ALL TESTS PASSED ===");
        return 0;
    } else {
        std::printf("=== %d TEST(S) FAILED ===\n", g_failed);
        return 1;
    }
}
