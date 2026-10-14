#include "rng.h"

#define RNG_MULTIPLIER 214013u
#define RNG_INCREMENT 2531011u

void rng_seed(Rng *rng, uint16_t seed)
{
    rng->state = seed;
}

int rng_next(Rng *rng)
{
    rng->state = rng->state * RNG_MULTIPLIER + RNG_INCREMENT;
    return (int)((rng->state >> 16) & RNG_MAX);
}
