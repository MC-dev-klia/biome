#include "sb_noise.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

static const int period_a[6] = { // 1 to 4 scaling
    1024 * 256, 256 * 256, 512 * 256, 
    512 * 256, 0, 128 * 256
};

uint64_t hash_key(int type, int octave_index,
    char half_octave, int x, int z) {
    x += 30000000;
    z += 30000000;
    uint64_t tot = ((uint64_t)x << 38) | ((uint64_t)z << 12) | (type << 6) 
        | (octave_index << 2) | (half_octave == 'A');
    tot *= 0x9E3779B97F4A7C15ULL;
    tot ^= (tot << 33);
    tot *= 0xFF51AFD7ED558CCDULL;
    tot ^= (tot >> 27);
    tot *= 0x94D049BB133111EBULL;
    tot ^= (tot >> 31);
    return tot;
}

uint64_t pos_hash(pos cur) {
    cur.x += 30000000;
    cur.z += 30000000;
    uint64_t tot = ((uint64_t)cur.x << 32) | (cur.z);
    tot *= 0x9E3779B97F4A7C15ULL;
    tot ^= (tot << 33);
    tot *= 0xFF51AFD7ED558CCDULL;
    tot ^= (tot >> 27);
    tot *= 0x94D049BB133111EBULL;
    tot ^= (tot >> 31);
    return tot;
}

void init_hash(hash_table *table, int capacity) { // capacity must be power of 2
    table->capacity = capacity;
    table->cur = 0;
    table->keys = (uint64_t*)malloc(capacity * sizeof(uint64_t));
    table->values = (int*)malloc(capacity * sizeof(int));
    table->hasvalue = (int*)calloc(capacity, sizeof(int));
}


void setvalue(hash_table *table, uint64_t key, int value) {
    if (table->cur + 1 > table->capacity) return;
    uint64_t mask = table->capacity - 1;
    int index = key & mask;
    for (int i = 0; i < table->capacity; i++) {
        if (!table->hasvalue[index]) {
            table->keys[index] = key;
            table->values[index] = value;
            table->hasvalue[index] = 1;
            table->cur++;
            return;
        }
        index++; 
        index &= mask;
    }
}

int getvalue(hash_table *table, uint64_t key) {
    uint64_t mask = table->capacity - 1;
    int index = key & mask;
    for (int i = 0; i < table->capacity; i++) {
        if (!table->hasvalue[index]) {
            return (int)(1<<30);
        }
        if (table->keys[index] == key) {
            return table->values[index];
        }
        index++; 
        index &= mask;
    }
    return (uint64_t)(1<<30);
}

void free_hash_table(hash_table *table)
{
    free(table->keys);
    free(table->values);
    free(table->hasvalue);
    table->keys = NULL;
    table->values = NULL;
    table->hasvalue = NULL;
    table->capacity = 0;
    table->cur = 0;
}

static const int octave_counts[6] = {
    2, 2, 9, 4, 0, 3
};



void sb_setseed(sbNoise *noise, uint64_t seed, int capacity) {
    noise->seed = seed;
    noise->initialized = 0;
    memset(noise->initialized_octaves, 0, sizeof(noise->initialized_octaves));
    init_hash(&noise->hash, capacity);
}

static void init_parameter(sbNoise *noise, int type, int octaves) {
    BiomeNoise b = {0};
    DoublePerlinNoise *d;
    int offset = 0;
    int count;
    for (int i = NP_TEMPERATURE; i < type; i++)
        offset += 2 * octave_counts[i];
    setClimateParaSeed(&b, noise->seed, 0, type, 2 * octaves); // 1 for lb
    d = &b.climate[type];
    count = d->octA.octcnt + d->octB.octcnt;
    memcpy(noise->octaves + offset, b.oct, count * sizeof(*noise->octaves));
    noise->climate[type] = *d;
    noise->climate[type].octA.octaves = noise->octaves + offset;
    noise->climate[type].octB.octaves =
        noise->octaves + offset + d->octA.octcnt;
    noise->initialized |= 1u << type;
    noise->initialized_octaves[type] = (unsigned char)d->octA.octcnt;
}

static void init(sbNoise *noise, int type, int octaves) {
    if (!(noise->initialized & (1u << type)) ||
        noise->initialized_octaves[type] < octaves)
        init_parameter(noise, type, octaves);
}

static inline void part(const DoublePerlinNoise *d, int octave_index,
    char half_octave, double x, double z, double *value) {
    const OctaveNoise *s;
    double mult;
    const PerlinNoise *q;
    if (half_octave == 'A') {
        s = &d->octA;
        mult = 1.0;
    }
    else if (half_octave == 'B') {
        s = &d->octB;
        mult = 337.0 / 331.0;
    }
    else abort();
    if (octave_index < 0 || octave_index >= s->octcnt) abort();
    q = s->octaves + octave_index;
    *value = d->amplitude * q->amplitude * samplePerlin(q,
        x * mult * q->lacunarity, 0, z * mult * q->lacunarity, 0, 0);
}

double sb_octave(sbNoise *noise, int type, int octave_index,
    char half_octave, double x, double z) {
    double value;
    init(noise, type, octave_index + 1);
    part(&noise->climate[type], octave_index, half_octave, x, z, &value);
    return value;
}

int sb_octave_int(sbNoise *noise, int type, int octave_index,
    char half_octave, int x, int z) {
    x/=4;
    z/=4;
    if (half_octave == 'A') {
        x = ((x % period_a[type]) + period_a[type]) % period_a[type];
        z = ((z % period_a[type]) + period_a[type]) % period_a[type];
    }
    uint64_t key = hash_key(type, octave_index, half_octave, x, z);
    int val = getvalue(&noise->hash, key);
    if (val != 1 << 30) {
        return val;
    }
    else {
        double value = sb_octave(noise, type,
            octave_index, half_octave, x, z);
        int res = value * 10000;
        setvalue(&noise->hash, key, res);
        return res;
    }
}

int sb_octave_int_sum(sbNoise *noise, int type, int octave_index,
    char half_octave, int x, int z) {
    int sum = 0;
    for (int i = 0; i <= octave_index; i++) {
        if (i == octave_index) {
            if (half_octave == 'A') {
                sum += sb_octave_int(noise, type, i, 'A', x, z);
            }
            else {
                sum += sb_octave_int(noise, type, i, 'A', x, z);
                sum += sb_octave_int(noise, type, i, 'B', x, z);
            }
        }
        else {
            sum += sb_octave_int(noise, type, i, 'A', x, z);
            sum += sb_octave_int(noise, type, i, 'B', x, z);
        }
    }
    return sum;
}

int floodfill(sbNoise *noise, valid is_valid, int x, int z, int step_size, int limit) {
    if (!is_valid(noise, x,z)) return 0;
    pos q[limit];
    hash_table visited;
    init_hash(&visited, limit * 4);
    int head = 0, tail = 0;
    int dx[] = {-step_size, step_size, 0, 0};
    int dz[] = {0, 0, -step_size, step_size};
    q[head++] = (pos){x,z};
    setvalue(&visited, pos_hash((pos){x, z}), 1);
    while (head > tail) {
        pos cur = q[tail++];
        for (int i = 0; i < 4; i++){
            int cx = dx[i];
            int cz = dz[i];
            if (getvalue(&visited, pos_hash((pos){cur.x + cx, cur.z + cz})) 
                != (1 << 30)){
                continue;
            }
            if (is_valid(noise, cur.x + cx, cur.z + cz)) {
                if (head >= limit || tail >= limit){ 
                    free_hash_table(&visited);
                    return limit;
                }                            
                setvalue(&visited, pos_hash((pos){cur.x + cx, cur.z + cz}), 1);
                q[head++] = (pos){cur.x + cx, cur.z + cz};
            }
            else {
                setvalue(&visited, pos_hash((pos){cur.x + cx, cur.z + cz}), 1);
            }
        }
    }
    free_hash_table(&visited);
    return head;
}

void free_all(sbNoise *noise) {
    free_hash_table(&noise->hash);
}