/* Bouchon de controle (voir stdint.h) : PRI*32 / SCN*32 version "long" comme ESP32. */
#include <stdint.h>
#include_next <inttypes.h>
#undef PRIu32
#undef PRIx32
#undef PRIX32
#undef PRId32
#undef PRIo32
#define PRIu32 "lu"
#define PRIx32 "lx"
#define PRIX32 "lX"
#define PRId32 "ld"
#define PRIo32 "lo"
#undef SCNu32
#undef SCNd32
#define SCNu32 "lu"
#define SCNd32 "ld"
