#include <stdio.h>
#include <stdint.h>

#include <time.h>

#define NOB_IMPLEMENTATION
#include "nob.h"

typedef struct {
    Nob_String_View key;
    size_t value;
    bool occupied;
} FreqKV;

typedef struct {
    FreqKV *items;
    size_t count;
    size_t capacity;
} FreqKVs;

FreqKV *find_key(FreqKVs haystack, Nob_String_View needle)
{
    for (size_t i = 0; i < haystack.count; ++i) {
        if (nob_sv_eq(haystack.items[i].key, needle)) {
            return &haystack.items[i];
        }
    }
    return NULL;
}

int compare_freqkv_count(const void *a, const void *b)
{
    const FreqKV *akv = a;
    const FreqKV *bkv = b;
    return (int)bkv->value - (int)akv->value;
}

double clock_get_secs(void)
{
    struct timespec ts = {0};
    int ret = clock_gettime(CLOCK_MONOTONIC, &ts);
    assert(ret == 0);
    return (double)ts.tv_sec + ts.tv_nsec*1e-9;
}

void naive_analysis(Nob_String_View content, const char *file_path)
{
    nob_log(NOB_INFO, "Analyzing %s linearly", file_path);
    nob_log(NOB_INFO, "  Size: %zu bytes", content.count);

    double begin = clock_get_secs();

    FreqKVs freq = {0}; // TODO: a bit of memory leak
    size_t count = 0;
    for (; content.count > 0; ++count) {
        content = nob_sv_trim_left(content);
        Nob_String_View token = nob_sv_chop_by_space(&content);

        FreqKV *kv = find_key(freq, token);
        if (kv) {
            kv->value += 1;
        } else {
            nob_da_append(&freq, ((FreqKV) {
                .key = token,
                .value = 1
            }));
        }
    }

    double end = clock_get_secs();

    nob_log(NOB_INFO, "  Tokens: %zu tokens", freq.count);
    qsort(freq.items, freq.count, sizeof(freq.items[0]), compare_freqkv_count);

    nob_log(NOB_INFO, "  Top 10 tokens");
    for (size_t i = 0; i < 10 && i < freq.count; ++i) {
        nob_log(NOB_INFO, "    %zu: "SV_Fmt" => %zu", i, SV_Arg(freq.items[i].key), freq.items[i].value);
    }
    nob_log(NOB_INFO, "  Elapsed time %.03lfs", end - begin);
}

uint32_t djb2(uint8_t *buf, size_t buf_size)
{
    uint32_t hash = 5381;

    for (size_t i = 0; i < buf_size; ++i) {
        hash = ((hash << 5) + hash) + (uint32_t)buf[i]; /* hash * 33 + c */
    }

    return hash;
}

#define hash_init(ht, cap) \
    do { \
        (ht)->items = malloc(sizeof(*(ht)->items)*cap); \
        memset((ht)->items, 0, sizeof(*(ht)->items)*cap); \
        (ht)->count = 0; \
        (ht)->capacity = (cap); \
    } while(0)

#define hash_find(ht, key, hash, eq)

bool hash_analysis(Nob_String_View content, const char *file_path)
{
    nob_log(NOB_INFO, "Analyzing %s with Hash Table", file_path);
    nob_log(NOB_INFO, "  Size: %zu bytes", content.count);

    double begin = clock_get_secs();

    FreqKVs ht = {0};
    hash_init(&ht, 1000*1000);

    size_t count = 0;
    for (; content.count > 0; ++count) {
        content = nob_sv_trim_left(content);
        Nob_String_View token = nob_sv_chop_by_space(&content);

        uint32_t h = djb2((uint8_t*)token.data, token.count)%ht.capacity;

        for (size_t i = 0; i < ht.capacity && ht.items[h].occupied && !nob_sv_eq(ht.items[h].key, token); ++i) {
            h = (h + 1)%ht.capacity;
        }

        if (ht.items[h].occupied) {
            if (!nob_sv_eq(ht.items[h].key, token)) {
                nob_log(NOB_ERROR, "Table overflow");
                return false;
            }
            ht.items[h].value += 1;
        } else {
            ht.items[h].occupied = true;
            ht.items[h].key = token;
            ht.items[h].value = 1;
        }
    }
    double end = clock_get_secs();

    FreqKVs freq = {0};
    for (size_t i = 0; i < ht.capacity; ++i) {
        if (ht.items[i].occupied) {
            nob_da_append(&freq, ht.items[i]);
        }
    }
    qsort(freq.items, freq.count, sizeof(freq.items[0]), compare_freqkv_count);

    nob_log(NOB_INFO, "  Tokens: %zu tokens", freq.count);
    qsort(freq.items, freq.count, sizeof(freq.items[0]), compare_freqkv_count);

    nob_log(NOB_INFO, "  Top 10 tokens");
    for (size_t i = 0; i < 10 && i < freq.count; ++i) {
        nob_log(NOB_INFO, "    %zu: "SV_Fmt" => %zu", i, SV_Arg(freq.items[i].key), freq.items[i].value);
    }
    nob_log(NOB_INFO, "  Elapsed time %.03lfs", end - begin);

    return true;
}

int main(int argc, char **argv)
{
    const char *program = nob_shift_args(&argc, &argv);

    if (argc <= 0) {
        nob_log(NOB_ERROR, "No input is provided");
        nob_log(NOB_INFO, "Usage: %s <input.txt>", program);
        return 1;
    }

    const char *file_path = nob_shift_args(&argc, &argv);
    Nob_String_Builder buf = {0};
    if (!nob_read_entire_file(file_path, &buf)) return 1;

    Nob_String_View content = {
        .data = buf.items,
        .count = buf.count,
    };

    naive_analysis(content, file_path);
    if (!hash_analysis(content, file_path)) return 1;

    return 0;
}
