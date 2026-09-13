/*
 * Exhaustive Switch 1.6.15 weather and Crop Fairy seed searches.
 *
 * This is a native, multithreaded reproduction of the predictor's
 * xxHash32 + JKISS path. Ranges use [start, end) semantics.
 */

#include <errno.h>
#include <inttypes.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define RANDOM_MODULUS UINT64_C(2147483647)
#define WEATHER_PERIOD UINT64_C(4294967294)
#define MAX_RESULTS 4096

#define XXH_PRIME32_1 UINT32_C(2654435761)
#define XXH_PRIME32_2 UINT32_C(2246822519)
#define XXH_PRIME32_3 UINT32_C(3266489917)
#define XXH_PRIME32_4 UINT32_C(668265263)
#define XXH_PRIME32_5 UINT32_C(374761393)

/* Signed results of GetDeterministicHashCode(string). */
#define HASH_LOCATION_WEATHER INT64_C(-1513201250)
#define HASH_SUMMER_RAIN_CHANCE INT64_C(-309161378)

enum search_mode {
    MODE_MAX_WET,
    MODE_EXACT_SPRING,
    MODE_LATE_FAIRY,
    MODE_LATE_NIGHT1_FAIRY
};

struct result_set {
    int score;
    uint64_t count;
    uint64_t values[MAX_RESULTS];
    size_t stored;
};

struct worker_args {
    enum search_mode mode;
    uint64_t start;
    uint64_t end;
    struct result_set result;
};

struct jkiss {
    uint32_t x;
    uint32_t y;
    uint32_t z;
    uint32_t c;
};

static uint32_t rotl32(uint32_t value, unsigned count)
{
    return (value << count) | (value >> (32 - count));
}

static uint32_t read32le(const int32_t values[5], size_t byte_offset)
{
    const uint32_t *words = (const uint32_t *)(const void *)values;
    return words[byte_offset / 4];
}

/* XXH32 over the five little-endian Int32 values used by getHashFromArray. */
static uint32_t xxh32_five(const int32_t values[5])
{
    uint32_t v1 = XXH_PRIME32_1 + XXH_PRIME32_2;
    uint32_t v2 = XXH_PRIME32_2;
    uint32_t v3 = 0;
    uint32_t v4 = 0 - XXH_PRIME32_1;
    uint32_t h;

#define XXH_ROUND(acc, input) \
    do { \
        (acc) += (input) * XXH_PRIME32_2; \
        (acc) = rotl32((acc), 13); \
        (acc) *= XXH_PRIME32_1; \
    } while (0)

    XXH_ROUND(v1, read32le(values, 0));
    XXH_ROUND(v2, read32le(values, 4));
    XXH_ROUND(v3, read32le(values, 8));
    XXH_ROUND(v4, read32le(values, 12));
    h = rotl32(v1, 1) + rotl32(v2, 7) + rotl32(v3, 12) + rotl32(v4, 18);
    h += 20;
    h += read32le(values, 16) * XXH_PRIME32_3;
    h = rotl32(h, 17) * XXH_PRIME32_4;
    h ^= h >> 15;
    h *= XXH_PRIME32_2;
    h ^= h >> 13;
    h *= XXH_PRIME32_3;
    h ^= h >> 16;

#undef XXH_ROUND
    return h;
}

static int32_t js_mod_i32(int64_t value)
{
    /* C and JavaScript both retain the dividend's sign for integer %. */
    return (int32_t)(value % (int64_t)RANDOM_MODULUS);
}

static uint32_t random_seed(int64_t a, int64_t b, int64_t c)
{
    int32_t values[5] = {
        js_mod_i32(a), js_mod_i32(b), js_mod_i32(c), 0, 0
    };
    return xxh32_five(values);
}

static void jkiss_init(struct jkiss *rng, uint32_t seed)
{
    rng->x = seed;
    rng->y = UINT32_C(987654321);
    rng->z = UINT32_C(43219876);
    rng->c = UINT32_C(6543217);
}

static uint32_t jkiss_next_internal(struct jkiss *rng)
{
    uint64_t t;
    rng->x = UINT32_C(314527869) * rng->x + UINT32_C(1234567);
    rng->y ^= rng->y << 5;
    rng->y ^= rng->y >> 7;
    rng->y ^= rng->y << 22;
    t = UINT64_C(4294584393) * rng->z + rng->c;
    rng->c = (uint32_t)(t >> 32);
    rng->z = (uint32_t)t;
    return rng->x + rng->y + rng->z;
}

static double jkiss_next_double(struct jkiss *rng)
{
    uint32_t a = jkiss_next_internal(rng) >> 6;
    uint32_t b = jkiss_next_internal(rng) >> 5;
    return ((double)a * 134217728.0 + (double)b) / 9007199254740992.0;
}

static int weather_roll(int64_t a, int64_t b, int64_t c, double chance)
{
    struct jkiss rng;
    jkiss_init(&rng, random_seed(a, b, c));
    return jkiss_next_double(&rng) < chance;
}

static int is_spring_rain(uint64_t id, int day)
{
    if (day == 3) return 1;
    if (day == 1 || day == 2 || day == 4 || day == 5 || day == 13 || day == 24) return 0;
    return weather_roll(HASH_LOCATION_WEATHER, (int64_t)id, day - 1, 0.183);
}

static int green_rain_day(uint64_t id)
{
    static const int days[8] = { 5, 6, 7, 14, 15, 16, 18, 23 };
    struct jkiss rng;
    jkiss_init(&rng, random_seed(777, (int64_t)id, 0));
    return days[jkiss_next_internal(&rng) % 8];
}

static int is_summer_wet(uint64_t id, int day)
{
    int absolute_day = day + 28;
    int green_day;
    if (day == 1 || day == 11 || day == 28) return 0;
    green_day = green_rain_day(id);
    if (day == green_day || day == 13 || day == 26) return 1;
    return weather_roll(absolute_day - 1, (int64_t)(id / 2),
                        HASH_SUMMER_RAIN_CHANCE,
                        0.12 + 0.003 * (day - 1));
}

static int is_spring_fairy(uint64_t id, int night)
{
    struct jkiss rng;
    int i;

    /* Stardew 1.6 rejects a Crop Fairy on a season's final night. */
    if (night < 1 || night >= 28 || is_spring_rain(id, night)) return 0;
    jkiss_init(&rng, random_seed(night + 1, (int64_t)(id / 2), 0));
    for (i = 0; i < 10; i++) (void)jkiss_next_double(&rng);
    return jkiss_next_double(&rng) < 0.01;
}

static uint32_t spring_mask(uint64_t id)
{
    uint32_t mask = 0;
    int day;
    for (day = 1; day <= 28; day++) {
        if (is_spring_rain(id, day)) mask |= UINT32_C(1) << (day - 1);
    }
    return mask;
}

static int wet_count(uint64_t id)
{
    int count = 0;
    int day;
    for (day = 1; day <= 28; day++) {
        count += is_spring_rain(id, day);
        count += is_summer_wet(id, day);
    }
    return count;
}

static int late_fairy_score(uint64_t id, int require_night1)
{
    int day;
    int fairy = 0;
    int score = 0;

    if (is_spring_rain(id, 6) || is_spring_rain(id, 7)) return -1;
    if (!is_spring_rain(id, 8) || !is_spring_rain(id, 9) || !is_spring_rain(id, 10)) return -1;

    if (require_night1) {
        fairy = is_spring_fairy(id, 1);
    } else {
        for (day = 1; day <= 27 && !fairy; day++) fairy = is_spring_fairy(id, day);
    }
    if (!fairy) return -1;

    for (day = 13; day <= 28; day++) score += is_spring_rain(id, day);
    return score;
}

static void add_result(struct result_set *result, uint64_t id, int score, int maximize)
{
    if (maximize && score > result->score) {
        result->score = score;
        result->count = 0;
        result->stored = 0;
    }
    if (!maximize || score == result->score) {
        result->count++;
        if (result->stored < MAX_RESULTS) result->values[result->stored++] = id;
    }
}

static void *search_worker(void *opaque)
{
    struct worker_args *args = opaque;
    const uint32_t exact_mask =
        (UINT32_C(1) << (3 - 1)) |
        (UINT32_C(1) << (6 - 1)) |
        (UINT32_C(1) << (7 - 1)) |
        (UINT32_C(1) << (8 - 1)) |
        (UINT32_C(1) << (9 - 1)) |
        (UINT32_C(1) << (10 - 1)) |
        (UINT32_C(1) << (18 - 1)) |
        (UINT32_C(1) << (19 - 1)) |
        (UINT32_C(1) << (20 - 1)) |
        (UINT32_C(1) << (21 - 1)) |
        (UINT32_C(1) << (22 - 1)) |
        (UINT32_C(1) << (28 - 1));
    uint64_t id;

    args->result.score = -1;
    for (id = args->start; id < args->end; id++) {
        int score;
        switch (args->mode) {
        case MODE_MAX_WET:
            score = wet_count(id);
            add_result(&args->result, id, score, 1);
            break;
        case MODE_EXACT_SPRING:
            if (spring_mask(id) == exact_mask) add_result(&args->result, id, 0, 0);
            break;
        case MODE_LATE_FAIRY:
        case MODE_LATE_NIGHT1_FAIRY:
            score = late_fairy_score(id, args->mode == MODE_LATE_NIGHT1_FAIRY);
            if (score >= 0) add_result(&args->result, id, score, 1);
            break;
        }
    }
    return NULL;
}

static void print_days(uint64_t id, int summer)
{
    int day;
    int first = 1;
    for (day = 1; day <= 28; day++) {
        int wet = summer ? is_summer_wet(id, day) : is_spring_rain(id, day);
        if (wet) {
            printf("%s%d", first ? "" : ",", day);
            first = 0;
        }
    }
    putchar('\n');
}

static void print_fairies(uint64_t id)
{
    int day;
    int first = 1;
    for (day = 1; day <= 27; day++) {
        if (is_spring_fairy(id, day)) {
            printf("%s%d", first ? "" : ",", day);
            first = 0;
        }
    }
    if (first) printf("none");
    putchar('\n');
}

static uint64_t parse_u64(const char *text, const char *label)
{
    char *end;
    unsigned long long value;
    errno = 0;
    value = strtoull(text, &end, 10);
    if (errno || *text == '\0' || *end != '\0') {
        fprintf(stderr, "Invalid %s: %s\n", label, text);
        exit(2);
    }
    return (uint64_t)value;
}

static void usage(const char *program)
{
    fprintf(stderr,
        "Usage:\n"
        "  %s weather SEED\n"
        "  %s max-wet [START END [JOBS]]\n"
        "  %s exact-spring [START END [JOBS]]\n"
        "  %s late-fairy [START END [JOBS]]\n"
        "  %s late-night1-fairy [START END [JOBS]]\n\n"
        "Ranges are start-inclusive/end-exclusive. Defaults cover the complete\n"
        "predictor period (Spring-only for exact-spring).\n",
        program, program, program, program, program);
}

int main(int argc, char **argv)
{
    enum search_mode mode;
    uint64_t start = 0;
    uint64_t end;
    long detected_jobs;
    int jobs;
    pthread_t *threads;
    struct worker_args *workers;
    struct result_set total;
    uint64_t span;
    int i;

    if (argc >= 2 && strcmp(argv[1], "weather") == 0) {
        uint64_t id;
        if (argc != 3) { usage(argv[0]); return 2; }
        id = parse_u64(argv[2], "seed");
        printf("seed=%" PRIu64 "\nspring=", id);
        print_days(id, 0);
        printf("summer=");
        print_days(id, 1);
        printf("spring_fairy=");
        print_fairies(id);
        printf("combined_wet=%d\n", wet_count(id));
        return 0;
    }
    if (argc < 2 || argc == 3 || argc > 5) { usage(argv[0]); return 2; }

    if (strcmp(argv[1], "max-wet") == 0) {
        mode = MODE_MAX_WET;
        end = WEATHER_PERIOD;
    } else if (strcmp(argv[1], "exact-spring") == 0) {
        mode = MODE_EXACT_SPRING;
        end = RANDOM_MODULUS;
    } else if (strcmp(argv[1], "late-fairy") == 0) {
        mode = MODE_LATE_FAIRY;
        end = WEATHER_PERIOD;
    } else if (strcmp(argv[1], "late-night1-fairy") == 0) {
        mode = MODE_LATE_NIGHT1_FAIRY;
        end = WEATHER_PERIOD;
    } else {
        usage(argv[0]);
        return 2;
    }

    if (argc >= 4) {
        start = parse_u64(argv[2], "start");
        end = parse_u64(argv[3], "end");
    }
    if (start >= end || end > WEATHER_PERIOD) {
        fprintf(stderr, "Range must satisfy 0 <= START < END <= %" PRIu64 "\n", WEATHER_PERIOD);
        return 2;
    }
    detected_jobs = sysconf(_SC_NPROCESSORS_ONLN);
    jobs = detected_jobs > 0 ? (int)detected_jobs : 1;
    if (argc == 5) jobs = (int)parse_u64(argv[4], "jobs");
    span = end - start;
    if (jobs < 1) jobs = 1;
    if ((uint64_t)jobs > span) jobs = (int)span;

    threads = calloc((size_t)jobs, sizeof(*threads));
    workers = calloc((size_t)jobs, sizeof(*workers));
    if (!threads || !workers) { perror("calloc"); return 1; }

    for (i = 0; i < jobs; i++) {
        workers[i].mode = mode;
        workers[i].start = start + (span * (uint64_t)i) / (uint64_t)jobs;
        workers[i].end = start + (span * (uint64_t)(i + 1)) / (uint64_t)jobs;
        if (pthread_create(&threads[i], NULL, search_worker, &workers[i]) != 0) {
            perror("pthread_create");
            return 1;
        }
    }

    memset(&total, 0, sizeof(total));
    total.score = -1;
    for (i = 0; i < jobs; i++) {
        size_t j;
        pthread_join(threads[i], NULL);
        if (mode != MODE_EXACT_SPRING && workers[i].result.score > total.score) {
            total.score = workers[i].result.score;
            total.count = 0;
            total.stored = 0;
        }
        if (mode == MODE_EXACT_SPRING || workers[i].result.score == total.score) {
            total.count += workers[i].result.count;
            for (j = 0; j < workers[i].result.stored && total.stored < MAX_RESULTS; j++) {
                total.values[total.stored++] = workers[i].result.values[j];
            }
        }
    }

    printf("range=[%" PRIu64 ",%" PRIu64 ") jobs=%d\n", start, end, jobs);
    if (mode == MODE_EXACT_SPRING) {
        printf("matches=%" PRIu64 "\n", total.count);
    } else {
        printf("best_score=%d matches=%" PRIu64 "\n", total.score, total.count);
    }
    for (i = 0; i < (int)total.stored; i++) printf("seed=%" PRIu64 "\n", total.values[i]);
    if (total.count > total.stored) printf("results_truncated_after=%d\n", MAX_RESULTS);

    free(workers);
    free(threads);
    return 0;
}
