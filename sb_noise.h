#ifndef SB_NOISE_H
#define SB_NOISE_H

#include "cubiomes/biomenoise.h"

/*
0: "NP_TEMPERATURE",
1: "NP_HUMIDITY",
2: "NP_CONTINENTALNESS",
3: "NP_EROSION",
5: "NP_WEIRDNESS"
*/

typedef struct {
    uint64_t *keys;
    int *values;
    int *hasvalue;
    int capacity;
    int cur;    
} hash_table;

typedef struct {
    uint64_t seed;
    unsigned int initialized;
    unsigned char initialized_octaves[6];
    DoublePerlinNoise climate[6];
    PerlinNoise octaves[40];
    hash_table hash;
} sbNoise;

void sb_setseed(sbNoise *noise, uint64_t seed, int capacity);
// capacity must be power of 2
double sb_octave(sbNoise *noise, int type, int octave_index, char half_octave, double x, double z);
int sb_octave_int(sbNoise *noise, int type, int octave_index, char half_octave, int x,int z);
int sb_octave_int_sum(sbNoise *noise, int type, int octave_index,
char half_octave, int x, int z);
void free_all(sbNoise *noise);

typedef int (*valid)(sbNoise *noise, int x, int z);
typedef struct {
    int x;
    int z;
} pos;
int floodfill(sbNoise *noise, valid is_valid, int x, int z, int step_size, int limit);
//limit must be power of 2


#endif