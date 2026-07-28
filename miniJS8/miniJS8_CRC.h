#ifndef JS8_CRC_H
#define JS8_CRC_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <malloc.h>
#include <string.h>
#include "miniJS8_const.h"


void miniJS8_checksum16(const char *line, char *out);

void miniJS8_checksum32(const char *line, char *out);

uint16_t miniJS8_CRC12(const uint8_t *data, size_t size);

#endif