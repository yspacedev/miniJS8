#ifndef JS8_PACK_H
#define JS8_PACK_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <malloc.h>
#include <string.h>
#include <ctype.h>
#include "miniJS8_const.h"

size_t   miniJS8_pack72bits(uint64_t value, uint8_t rem, uint8_t outMsg[12]);
uint32_t miniJS8_packCallsign(const char *value, bool *pPortable);
uint16_t miniJS8_packGrid(const char *value);
uint8_t  miniJS8_packNum(const char *num, bool *ok);
uint8_t  miniJS8_packCmd(uint8_t cmd, uint8_t num, bool *pPackedNum);
uint64_t miniJS8_packAlphaNumeric50(const char *value);
size_t   miniJS8_packCompoundFrame(const char *callsign, uint8_t type, uint16_t num, uint8_t bits3, uint8_t outMsg[12]);

size_t miniJS8_packHeartbeatMessage(const char *line, const char *mycall, uint8_t outMsg[12]);
size_t miniJS8_packCompoundMessage(const char *line, uint8_t outMsg[12]); /* NOTE: simplified from earlier stub -- see patch note */
size_t miniJS8_packDirectedMessage(
    const char *line, const char *mycall,
    char dirTo[16], bool *dirToCompound,
    char dirCmd[16], /* widened from [8] -- see patch note */
    char dirNum[8],
    uint8_t outMsg[12]);

size_t miniJS8_packDataMessage(const char *input, uint8_t outMsg[12]);
size_t miniJS8_packFastDataMessage(const char *input, uint8_t outMsg[12]);
size_t miniJS8_packHuffMessage(const char *input, uint8_t prefixBits, size_t prefixCount, uint8_t outMsg[12]);

/* STUB: JSC::compress deferred per your decision -- returns 0/empty until implemented */
size_t miniJS8_packCompressedMessage(const char *input, uint8_t prefixBits, size_t prefixCount, uint8_t outMsg[12]);

#endif