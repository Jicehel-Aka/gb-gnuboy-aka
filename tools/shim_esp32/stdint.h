/* Bouchon de controle : fait de uint32_t/int32_t des "long" comme sur ESP32 (xtensa),
 * pour que gcc detecte sur PC les %u/%d incorrects (-Werror=format d'ESP-IDF). */
#include_next <stdint.h>
#define uint32_t unsigned long
#define int32_t long
