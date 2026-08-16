#ifndef PWMK_PERSISTENCE_H
#define PWMK_PERSISTENCE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

bool persistence_init(size_t image_size);
bool persistence_restore(uint8_t *image, size_t image_size);
bool persistence_commit(const uint8_t *image, size_t image_size);
bool persistence_rebuild(const uint8_t *image, size_t image_size);

#endif // PWMK_PERSISTENCE_H
