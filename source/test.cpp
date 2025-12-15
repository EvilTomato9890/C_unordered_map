// tests_unordered_map.cpp
//
// Набор тестов для библиотеки unordered_map (u_map).
//
// Сборка (пример, под *nix):
//   g++ -std=c++17 -O2 -I. tests_unordered_map.cpp unordered_map.cpp logger.cpp -o u_map_tests
//   ./u_map_tests
//
// На Windows (MSVC) просто добавь этот файл в проект рядом с исходниками библиотеки.
//
// Важно: библиотека использует error_handler.h / logger.h / asserts.h —
// при сборке тестов должны собираться те же зависимости, что и у unordered_map.cpp.

#include "unordered_map.h"

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <unordered_map>
#include <random>
#include <vector>

#if defined(_MSC_VER)
    #include <malloc.h>
#endif

//------------------------------------------------------------------------------
// Утилиты
//------------------------------------------------------------------------------

static bool is_pow2(size_t x) {
    return x != 0 && (x & (x - 1)) == 0;
}

static void* aligned_malloc_portable(size_t alignment, size_t size) {
#if defined(_MSC_VER)
    return _aligned_malloc(size, alignment);
#else
    void* p = nullptr;
    // posix_memalign требует alignment степенью двойки и кратным sizeof(void*)
    if (alignment < sizeof(void*)) alignment = sizeof(void*);
    int rc = posix_memalign(&p, alignment, size);
    return (rc == 0) ? p : nullptr;
#endif
}

static void aligned_free_portable(void* p) {
#if defined(_MSC_VER)
    _aligned_free(p);
#else
    free(p);
#endif
}

//------------------------------------------------------------------------------
// Хэши/сравнение для uint32_t / uint64_t
//------------------------------------------------------------------------------

static size_t hash_u32_good(const void* k) {
    // библиотека сама делает mix_hash, поэтому можно простенько
    return (size_t)(*(const uint32_t*)k);
}

static size_t hash_u32_const(const void* /*k*/) {
    // намеренно плохой хэш -> тестируем пробирование/коллизии
    return 1u;
}

static bool cmp_u32(const void* a, const void* b) {
    return *(const uint32_t*)a == *(const uint32_t*)b;
}

static size_t hash_u64_good(const void* k) {
    const uint64_t x = *(const uint64_t*)k;
    return (size_t)x;
}

static bool cmp_u64(const void* a, const void* b) {
    return *(const uint64_t*)a == *(const uint64_t*)b;
}

//------------------------------------------------------------------------------
// Мини-фреймворк тестов
//------------------------------------------------------------------------------

static int g_failed = 0;

#define CHECK(cond) do {                                                     \
    if (!(cond)) {                                                           \
        std::fprintf(stderr, "[FAIL] %s:%d: %s\n", __FILE__, __LINE__, #cond);\
        ++g_failed;                                                          \
        return;                                                              \
    }                                                                        \
} while (0)

#define CHECK_EQ(a,b) CHECK((a) == (b))
#define CHECK_NE(a,b) CHECK((a) != (b))

#define CHECK_OK(err)   CHECK((err) == HM_ERR_OK)
#define CHECK_NF(err)   CHECK((err) == HM_ERR_NOT_FOUND)
#define CHECK_FULL(err) CHECK((err) == HM_ERR_FULL)

//------------------------------------------------------------------------------
// Тесты
//------------------------------------------------------------------------------

static void test_basic_ops() {
    std::puts("[RUN ] test_basic_ops");

    u_map_t m {};
    CHECK_OK(SIMPLE_U_MAP_INIT(&m, 8, uint32_t, uint32_t, hash_u32_good, cmp_u32));

    CHECK(is_pow2(u_map_capacity(&m)));
    CHECK_EQ(u_map_size(&m), 0u);
    CHECK(u_map_is_empty(&m));

    // insert
    uint32_t k = 7, v = 100;
    CHECK_OK(u_map_insert_elem(&m, &k, &v));
    CHECK_EQ(u_map_size(&m), 1u);
    CHECK(!u_map_is_empty(&m));

    // get
    uint32_t out = 0;
    CHECK(u_map_get_elem(&m, &k, &out));
    CHECK_EQ(out, 100u);

    // update existing (size must not change)
    v = 200;
    CHECK_OK(u_map_insert_elem(&m, &k, &v));
    CHECK_EQ(u_map_size(&m), 1u);
    out = 0;
    CHECK(u_map_get_elem(&m, &k, &out));
    CHECK_EQ(out, 200u);

    // existence-only get
    CHECK(u_map_get_elem(&m, &k, nullptr));

    // remove
    uint32_t removed = 0;
    CHECK_OK(u_map_remove_elem(&m, &k, &removed));
    CHECK_EQ(removed, 200u);
    CHECK_EQ(u_map_size(&m), 0u);
    CHECK(u_map_is_empty(&m));
    CHECK(!u_map_get_elem(&m, &k, &out));
    CHECK_NF(u_map_remove_elem(&m, &k, nullptr));

    CHECK_OK(u_map_destroy(&m));

    std::puts("[OK  ] test_basic_ops");
}

static void test_collisions_constant_hash() {
    std::puts("[RUN ] test_collisions_constant_hash");

    u_map_t m {};
    CHECK_OK(SIMPLE_U_MAP_INIT(&m, 32, uint32_t, uint32_t, hash_u32_const, cmp_u32));

    constexpr uint32_t N = 2000;
    for (uint32_t i = 0; i < N; ++i) {
        uint32_t key = i;
        uint32_t val = i * 3u + 1u;
        CHECK_OK(u_map_insert_elem(&m, &key, &val));
    }
    CHECK_EQ(u_map_size(&m), (size_t)N);

    // verify all
    for (uint32_t i = 0; i < N; ++i) {
        uint32_t key = i;
        uint32_t out = 0;
        CHECK(u_map_get_elem(&m, &key, &out));
        CHECK_EQ(out, i * 3u + 1u);
    }

    // remove every 3rd
    for (uint32_t i = 0; i < N; i += 3) {
        uint32_t key = i;
        CHECK_OK(u_map_remove_elem(&m, &key, nullptr));
    }
    CHECK_EQ(u_map_size(&m), (size_t)(N - (N + 2) / 3));

    // verify removed / remaining
    for (uint32_t i = 0; i < N; ++i) {
        uint32_t key = i;
        uint32_t out = 0;
        bool ok = u_map_get_elem(&m, &key, &out);
        if (i % 3 == 0) {
            CHECK(!ok);
        } else {
            CHECK(ok);
            CHECK_EQ(out, i * 3u + 1u);
        }
    }

    // re-insert removed with new values + add some new keys
    for (uint32_t i = 0; i < N; i += 3) {
        uint32_t key = i;
        uint32_t val = 999999u ^ i;
        CHECK_OK(u_map_insert_elem(&m, &key, &val));
    }
    for (uint32_t i = N; i < N + 200; ++i) {
        uint32_t key = i;
        uint32_t val = i + 123u;
        CHECK_OK(u_map_insert_elem(&m, &key, &val));
    }

    // verify again
    for (uint32_t i = 0; i < N + 200; ++i) {
        uint32_t key = i;
        uint32_t out = 0;
        CHECK(u_map_get_elem(&m, &key, &out));
        if (i < N && i % 3 == 0) {
            CHECK_EQ(out, 999999u ^ i);
        } else if (i < N) {
            CHECK_EQ(out, i * 3u + 1u);
        } else {
            CHECK_EQ(out, i + 123u);
        }
    }

    CHECK_OK(u_map_destroy(&m));
    std::puts("[OK  ] test_collisions_constant_hash");
}

static void test_fuzz_against_std_unordered_map() {
    std::puts("[RUN ] test_fuzz_against_std_unordered_map");

    u_map_t m {};
    CHECK_OK(SIMPLE_U_MAP_INIT(&m, 64, uint32_t, uint32_t, hash_u32_good, cmp_u32));

    std::unordered_map<uint32_t, uint32_t> ref;

    std::mt19937 rng(0xC0FFEEu);
    std::uniform_int_distribution<uint32_t> key_dist(0, 5000);
    std::uniform_int_distribution<uint32_t> val_dist(0, 0xFFFFFFFFu);
    std::uniform_int_distribution<int> op_dist(0, 2); // 0 insert/update, 1 remove, 2 get

    constexpr int OPS = 60000;

    for (int i = 0; i < OPS; ++i) {
        uint32_t k = key_dist(rng);
        int op = op_dist(rng);

        if (op == 0) {
            uint32_t v = val_dist(rng);
            hm_error_t err = u_map_insert_elem(&m, &k, &v);
            CHECK_OK(err);
            ref[k] = v;
        } else if (op == 1) {
            hm_error_t err = u_map_remove_elem(&m, &k, nullptr);
            auto it = ref.find(k);
            if (it == ref.end()) {
                CHECK_NF(err);
            } else {
                CHECK_OK(err);
                ref.erase(it);
            }
        } else {
            uint32_t out = 0;
            bool ok = u_map_get_elem(&m, &k, &out);
            auto it = ref.find(k);
            if (it == ref.end()) {
                CHECK(!ok);
            } else {
                CHECK(ok);
                CHECK_EQ(out, it->second);
            }
        }

        CHECK_EQ(u_map_size(&m), ref.size());
    }

    // финальная сверка: все элементы ref должны быть доступны
    for (const auto& [k, v] : ref) {
        uint32_t out = 0;
        CHECK(u_map_get_elem(&m, &k, &out));
        CHECK_EQ(out, v);
    }

    CHECK_OK(u_map_destroy(&m));
    std::puts("[OK  ] test_fuzz_against_std_unordered_map");
}

static void test_static_init() {
    std::puts("[RUN ] test_static_init");

    constexpr size_t requested_capacity = 300; // округлится вниз до 256
    const size_t bytes = u_map_required_bytes(requested_capacity,
                                              sizeof(uint64_t), alignof(uint64_t),
                                              sizeof(uint64_t), alignof(uint64_t));
    CHECK(bytes != 0);

    const size_t alignment = std::max({(size_t)alignof(uint64_t),
                                      (size_t)alignof(uint64_t),
                                      (size_t)alignof(elem_state_t)});

    void* buf = aligned_malloc_portable(alignment, bytes);
    CHECK(buf != nullptr);
    std::memset(buf, 0, bytes);

    u_map_t m {};
    CHECK_OK(u_map_static_init(&m, buf, requested_capacity,
                              sizeof(uint64_t), alignof(uint64_t),
                              sizeof(uint64_t), alignof(uint64_t),
                              hash_u64_good, cmp_u64));

    CHECK(is_pow2(u_map_capacity(&m)));
    CHECK(u_map_capacity(&m) <= requested_capacity);
    CHECK_EQ(u_map_size(&m), 0u);

    // простые операции
    for (uint64_t i = 1; i <= 100; ++i) {
        uint64_t k = i;
        uint64_t v = i * 10;
        CHECK_OK(u_map_insert_elem(&m, &k, &v));
    }
    CHECK_EQ(u_map_size(&m), 100u);

    for (uint64_t i = 1; i <= 100; ++i) {
        uint64_t k = i, out = 0;
        CHECK(u_map_get_elem(&m, &k, &out));
        CHECK_EQ(out, i * 10);
    }

    CHECK_OK(u_map_destroy(&m)); // для static должен быть безопасным
    aligned_free_portable(buf);

    std::puts("[OK  ] test_static_init");
}

static void test_copy_smart_and_raw() {
    std::puts("[RUN ] test_copy_smart_and_raw");

    u_map_t src {};
    CHECK_OK(SIMPLE_U_MAP_INIT(&src, 64, uint32_t, uint32_t, hash_u32_const, cmp_u32));

    for (uint32_t i = 0; i < 1000; ++i) {
        uint32_t k = i;
        uint32_t v = i ^ 0xA5A5A5A5u;
        CHECK_OK(u_map_insert_elem(&src, &k, &v));
    }

    // smart copy
    u_map_t smart {};
    CHECK_OK(u_map_smart_copy(&smart, &src));
    CHECK_EQ(u_map_size(&smart), u_map_size(&src));

    // raw copy
    u_map_t raw {};
    CHECK_OK(u_map_raw_copy(&raw, &src));
    CHECK_EQ(u_map_size(&raw), u_map_size(&src));

    // сверка значений + независимость (после изменения src копии не должны поменяться)
    for (uint32_t i = 0; i < 1000; ++i) {
        uint32_t k = i;

        uint32_t s = 0, a = 0, b = 0;
        CHECK(u_map_get_elem(&src,   &k, &s));
        CHECK(u_map_get_elem(&smart, &k, &a));
        CHECK(u_map_get_elem(&raw,   &k, &b));
        CHECK_EQ(s, a);
        CHECK_EQ(s, b);
    }

    // меняем src
    for (uint32_t i = 0; i < 200; ++i) {
        uint32_t k = i;
        uint32_t v = 123456u + i;
        CHECK_OK(u_map_insert_elem(&src, &k, &v));
    }

    // копии должны хранить старые значения для i<200
    for (uint32_t i = 0; i < 200; ++i) {
        uint32_t k = i;
        uint32_t out = 0;
        CHECK(u_map_get_elem(&smart, &k, &out));
        CHECK_EQ(out, (i ^ 0xA5A5A5A5u));

        CHECK(u_map_get_elem(&raw, &k, &out));
        CHECK_EQ(out, (i ^ 0xA5A5A5A5u));
    }

    CHECK_OK(u_map_destroy(&raw));
    CHECK_OK(u_map_destroy(&smart));
    CHECK_OK(u_map_destroy(&src));

    std::puts("[OK  ] test_copy_smart_and_raw");
}

static size_t round_up_to_test(size_t x, size_t a) {
    if (a == 0) return x;
    size_t r = x % a;
    return r ? (x + (a - r)) : x;
}

static void test_read_arr_to_u_map() {
    std::puts("[RUN ] test_read_arr_to_u_map");

    // специально делаем value_align больше key_size, чтобы проверить паддинг
    using K = uint32_t;
    using V = double;

    u_map_t m {};
    CHECK_OK(u_map_init(&m, 64,
                        sizeof(K), alignof(K),
                        sizeof(V), alignof(V),
                        hash_u32_good, cmp_u32));

    const size_t key_part = sizeof(K);
    const size_t value_off = round_up_to_test(key_part, alignof(V));
    const size_t pair_stride = value_off + sizeof(V);

    constexpr size_t N = 50;
    std::vector<unsigned char> buf(pair_stride * N, 0);

    for (size_t i = 0; i < N; ++i) {
        K k = (K)(1000 + i);
        V v = (V)(0.5 * (double)i);

        std::memcpy(buf.data() + i * pair_stride, &k, sizeof(K));
        std::memcpy(buf.data() + i * pair_stride + value_off, &v, sizeof(V));
    }

    CHECK_OK(read_arr_to_u_map(&m, buf.data(), N));
    CHECK_EQ(u_map_size(&m), N);

    for (size_t i = 0; i < N; ++i) {
        K k = (K)(1000 + i);
        V out = 0;
        CHECK(u_map_get_elem(&m, &k, &out));
        CHECK(out == (V)(0.5 * (double)i));
    }

    CHECK_OK(u_map_destroy(&m));
    std::puts("[OK  ] test_read_arr_to_u_map");
}

//------------------------------------------------------------------------------
// main
//------------------------------------------------------------------------------

int main() {
    test_basic_ops();
    test_collisions_constant_hash();
    test_fuzz_against_std_unordered_map();
    test_static_init();
    test_copy_smart_and_raw();
    test_read_arr_to_u_map();

    if (g_failed == 0) {
        std::puts("\nALL TESTS PASSED ✅");
        return 0;
    }

    std::fprintf(stderr, "\nFAILED: %d test(s)\n", g_failed);
    return 1;
}
