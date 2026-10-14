#ifndef JT_CORE_RNG_H
#define JT_CORE_RNG_H

/*
 * The Microsoft C runtime rand()/srand() the original links: a 32-bit
 * linear congruential generator that returns bits 16-30 of its state. Kept
 * so that spawn positions and speeds follow the original's distribution.
 */

#include <stdint.h>

#define RNG_MAX 0x7FFF

typedef struct {
    uint32_t state;
} Rng;

/* Like srand(), only the low 16 bits of the seed are used. */
void rng_seed(Rng *rng, uint16_t seed);

/* Like rand(): 0 to RNG_MAX. */
int rng_next(Rng *rng);

#endif
