//CRC utilities

#include "miniJS8_CRC.h"
#include <string.h>

/* ============================================================
 * CRC-16/KERMIT -- standard, catalogued algorithm (poly=0x1021, init=0,
 * refin=refout=true, xorout=0). Implemented via the well-known reflected
 * form using the bit-reversed poly 0x8408 (this is the same constant used
 * in countless XMODEM/Kermit CRC16 implementations). High confidence --
 * this is public, standardized, not JS8-specific.
 * ============================================================ */
static uint16_t crc16_kermit(const uint8_t *data, size_t len) {
    uint16_t crc = 0x0000;
    for (size_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i];
        for (int b = 0; b < 8; b++) {
            if (crc & 1) {
                crc = (uint16_t)((crc >> 1) ^ 0x8408);
            } else {
                crc = (uint16_t)(crc >> 1);
            }
        }
    }
    return crc; /* xorout = 0, nothing further to do */
}

/* ============================================================
 * CRC-32/BZIP2 -- standard, catalogued algorithm (poly=0x04C11DB7,
 * init=0xFFFFFFFF, refin=refout=false, xorout=0xFFFFFFFF). Same core
 * bit-serial algorithm as classic non-reflected CRC-32, used by the bzip2
 * file format itself. High confidence.
 * ============================================================ */
static uint32_t crc32_bzip2(const uint8_t *data, size_t len) {
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < len; i++) {
        crc ^= ((uint32_t)data[i]) << 24;
        for (int b = 0; b < 8; b++) {
            if (crc & 0x80000000u) {
                crc = (crc << 1) ^ 0x04C11DB7u;
            } else {
                crc = crc << 1;
            }
        }
    }
    return crc ^ 0xFFFFFFFFu;
}

/* ============================================================
 * pack16bits / pack32bits -- exact port, base-41 fixed-width encoding
 * ============================================================ */
static void miniJS8_pack16bits(uint16_t packed, char out[4]) {
    uint16_t d0 = (uint16_t)(packed / (MINIJS8_NALPHABET * MINIJS8_NALPHABET));
    uint16_t d1 = (uint16_t)((packed - d0 * (MINIJS8_NALPHABET * MINIJS8_NALPHABET)) / MINIJS8_NALPHABET);
    uint16_t d2 = (uint16_t)(packed % MINIJS8_NALPHABET);
    out[0] = miniJS8_alphabet[d0];
    out[1] = miniJS8_alphabet[d1];
    out[2] = miniJS8_alphabet[d2];
    out[3] = '\0';
}

static void miniJS8_pack32bits(uint32_t packed, char out[7]) {
    uint16_t a = (uint16_t)((packed & 0xFFFF0000u) >> 16);
    uint16_t b = (uint16_t)(packed & 0xFFFFu);
    char bufA[4], bufB[4];
    miniJS8_pack16bits(a, bufA);
    miniJS8_pack16bits(b, bufB);
    memcpy(out, bufA, 3);
    memcpy(out + 3, bufB, 3);
    out[6] = '\0';
}

/* ============================================================
 * checksum16 / checksum32
 * ============================================================ */
void miniJS8_checksum16(const char *line, char out[4]) {
    size_t len = strlen(line);
    uint16_t crc = crc16_kermit((const uint8_t *)line, len);
    /* NOTE: the original's "pad with spaces if length < 3" branch is dead
     * code -- pack16bits always produces exactly 3 chars, so it's omitted
     * here (same category as the JS8_NO_MULTILINE dead branch earlier). */
    miniJS8_pack16bits(crc, out);
}

void miniJS8_checksum32(const char *line, char out[7]) {
    size_t len = strlen(line);
    uint32_t crc = crc32_bzip2((const uint8_t *)line, len);
    /* same dead-code note: pack32bits is always exactly 6 chars */
    miniJS8_pack32bits(crc, out);
}

/* ============================================================
 * UNCONFIRMED -- CRC12 (used by the future encode() step, not buildFrames)
 *
 * This ports boost::augmented_crc<12, 0xC06>, a DIFFERENT library from
 * CRCpp (used above), with different semantics: it's a "bare" bit-serial
 * CRC -- no init value beyond 0, no reflection, no final XOR -- computed
 * as if the message were followed by 12 zero bits (hence "augmented").
 * Because 12 isn't a byte-multiple width, there's real ambiguity in how
 * Boost handles bit alignment internally that I can't fully verify without
 * the actual library source or a known-good test vector. The implementation
 * below is the standard textbook bit-serial formulation (Williams' "A
 * Painless Guide to CRC Error Detection Algorithms", the classic
 * shift-register simulation), which should be *mathematically* equivalent
 * to Boost's intent, but please verify against a real JS8Call-encoded
 * frame before trusting this for on-air use.
 * ============================================================ */
static uint16_t augmented_crc(const uint8_t *data, size_t size, int bits, uint16_t poly) {
    uint32_t reg = 0;
    uint32_t mask = (uint32_t)((1u << bits) - 1);

    for (size_t byteIdx = 0; byteIdx < size; byteIdx++) {
        for (int bitIdx = 7; bitIdx >= 0; bitIdx--) {
            uint32_t inBit = (uint32_t)((data[byteIdx] >> bitIdx) & 1u);
            uint32_t msb = (reg >> (bits - 1)) & 1u;
            reg = ((reg << 1) | inBit) & mask;
            if (msb) reg ^= poly;
        }
    }
    /* augmentation: flush `bits` zero bits through the register */
    for (int i = 0; i < bits; i++) {
        uint32_t msb = (reg >> (bits - 1)) & 1u;
        reg = (reg << 1) & mask;
        if (msb) reg ^= poly;
    }
    return (uint16_t)reg;
}

uint16_t miniJS8_CRC12(const uint8_t *data, size_t size) {
    return augmented_crc(data, size, 12, 0xC06) ^ 42;
}