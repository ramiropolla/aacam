#include <inttypes.h>

uint8_t *pgm_read(const char *fname, int *width, int *height);
int pgm_write(const char *fname, uint8_t *pix, int width, int height);
