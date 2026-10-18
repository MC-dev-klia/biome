#include "sb_noise.h"
#include <stdio.h>
#include <inttypes.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    int value;
    int x;
    int z;
} result;






// gcc main.c sb_noise.c cubiomes/libcubiomes.a -O3 -pthread -lm -fwrapv -o test

static int is_valid(sbNoise *noise, int x, int z)
{
    if (sb_octave_int_sum(noise, NP_WEIRDNESS, 2, 'B', x, z) < 5000)
        return false;
    if (sb_octave_int_sum(noise, NP_CONTINENTALNESS, 2, 'B', x, z) < 300)
        return false;
    if (sb_octave_int_sum(noise, NP_EROSION, 3, 'B', x, z) > -4000)
        return false;
    return true;
}

result pass_seed(uint64_t seed) {
    sbNoise noise = {0};
    sb_setseed(&noise, seed, 32768);

    int floodfill_size = floodfill(
        &noise, is_valid, 0, 0, 16, 32768);
    free_all(&noise);
    if (floodfill_size <= 1000)
        return (result){0,0,0};
    return (result){floodfill_size,0,0};
}


















#define MAX_MATCHES 10
#define REPORT_INTERVAL UINT64_C(100000)
#define START UINT64_C(0)
#define SEEDS UINT64_C(10000000)
enum { THREAD_COUNT = 12 };
typedef struct {
    atomic_uint_fast64_t next_seed;
    atomic_uint_fast64_t checked;
    atomic_uint_fast64_t matches;
    atomic_bool stop;
    uint64_t end_seed;
    pthread_mutex_t report_mutex;
} Reporter;



static void *worker(void *arg) {
    Reporter *reporter = arg;
    while (!atomic_load(&reporter->stop)) {
        uint64_t seed = atomic_fetch_add(&reporter->next_seed, 1);
        if (seed >= reporter->end_seed) break;
        result res = pass_seed(seed);
        if (res.value > 0) {
            uint64_t match_number =
                atomic_fetch_add(&reporter->matches, 1) + 1;
            if (match_number <= MAX_MATCHES) {
                pthread_mutex_lock(&reporter->report_mutex);
                printf("[GOT RESULT] seed=%" PRIu64
                    " value=%d     Pos: (%d,%d)\n", seed, res.value, res.x, res.z);
                pthread_mutex_unlock(&reporter->report_mutex);
            }
            if (match_number >= MAX_MATCHES)
                atomic_store(&reporter->stop, true);
        }
        uint64_t checked = atomic_fetch_add(&reporter->checked, 1) + 1;
        if (checked % REPORT_INTERVAL == 0) {
            pthread_mutex_lock(&reporter->report_mutex);
            printf("Scanned %" PRIu64 "\n",
                checked);
            pthread_mutex_unlock(&reporter->report_mutex);
        }
    }
}

int main() {
    Reporter reporter;
    atomic_init(&reporter.next_seed, START);
    atomic_init(&reporter.checked, 0);
    atomic_init(&reporter.matches, 0);
    atomic_init(&reporter.stop, false);
    reporter.end_seed = START + SEEDS;
    pthread_mutex_init(&reporter.report_mutex, NULL);
    pthread_t threads[THREAD_COUNT];
    for (int i = 0; i < THREAD_COUNT; i++) {
        pthread_create(&threads[i], NULL, worker, &reporter);
    }
    for (int i = 0; i < THREAD_COUNT; i++) 
        pthread_join(threads[i], NULL);
    pthread_mutex_destroy(&reporter.report_mutex);
}