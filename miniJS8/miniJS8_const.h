#ifndef JS8_CONST_H
#define JS8_CONST_H

//Most constants and lookup tables for miniJS8

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <malloc.h>
#include <string.h>

#define MINIJS8_LINE_BUFSIZE 256
#define MINIJS8_MAX_CALLSIGNS 8

#define JS8_NUM_SYMBOLS    79
#define JS8_RX_SAMPLE_RATE 12000

#define JS8A_SYMBOL_SAMPLES 1920
#define JS8A_TX_SECONDS     15
#define JS8A_START_DELAY_MS 500

#define JS8B_SYMBOL_SAMPLES 1200
#define JS8B_TX_SECONDS     10
#define JS8B_START_DELAY_MS 200

#define JS8C_SYMBOL_SAMPLES 600
#define JS8C_TX_SECONDS     6
#define JS8C_START_DELAY_MS 100

#define JS8E_SYMBOL_SAMPLES 3840
#define JS8E_TX_SECONDS     30
#define JS8E_START_DELAY_MS 500

#define JS8I_SYMBOL_SAMPLES 384
#define JS8I_TX_SECONDS     4
#define JS8I_START_DELAY_MS 100

#define MINIJS8_NBASECALL ((uint32_t)(37u * 36u * 10u * 27u * 27u * 27u))
#define MINIJS8_NBASEGRID ((uint16_t)(180 * 180))
#define MINIJS8_NUSERGRID ((uint16_t)(MINIJS8_NBASEGRID + 10))
#define MINIJS8_NMAXGRID  ((uint16_t)((1 << 15) - 1))

typedef enum {
    MINIJS8_FRAME_TYPE_UNKNOWN            = 255, // [11111111] <- only used as a sentinel
    MINIJS8_FRAME_TYPE_HEARTBEAT          = 0,   // [000]
    MINIJS8_FRAME_TYPE_COMPOUND           = 1,   // [001]
    MINIJS8_FRAME_TYPE_COMPOUND_DIRECTED  = 2,   // [010]
    MINIJS8_FRAME_TYPE_DIRECTED           = 3,   // [011]
    MINIJS8_FRAME_TYPE_DATA               = 4,   // [10X] // but this only encodes the first 2 msb bits and drops the lsb
    MINIJS8_FRAME_TYPE_DATA_COMPRESSED    = 6,   // [11X] // but this only encodes the first 2 msb bits and drops the lsb
} miniJS8_InnerFrameType;

extern const char miniJS8_alphabet72[];
extern const char miniJS8_alphanumeric[];
extern const char miniJS8_alphabet[];
#define MINIJS8_NALPHABET 41

static int miniJS8_alphanumericIndex(char c) {
    const char *p = strchr(miniJS8_alphanumeric, c);
    return p ? (int)(p - miniJS8_alphanumeric) : -1;
}

enum miniJS8_costas_type {
    COSTAS_ORIGINAL,
    COSTAS_MODIFIED
};

typedef struct costas_s {uint8_t x[3][7];} miniJS8_costas_t;

extern const miniJS8_costas_t costas_arrays[2];

enum miniJS8_submodes{
    SUBMODE_NORMAL,
    SUBMODE_FAST,
    SUBMODE_TURBO,
    SUBMODE_SLOW,
    SUBMODE_ULTRA,
};

//only toneSpacing, startDelayMS, period, and txDuration are required for transmission
typedef struct {
    const char* name;
    unsigned int symbolSamples;
    unsigned int startDelayMS;
    unsigned int period;
    enum miniJS8_costas_type costas;
    int framesForSymbols;
    double toneSpacing;
    double ratio;
    double txDuration;
    unsigned int toneDurationMS;
} miniJS8_data_t;

extern const miniJS8_data_t miniJS8_submodes[5];

//fill in remaining fields (this could likely be done at compile-time)
static void miniJS8_compute_submode(miniJS8_data_t *data){
    data->framesForSymbols = JS8_NUM_SYMBOLS * data->symbolSamples;
    data->toneSpacing = JS8_RX_SAMPLE_RATE / (double)data->symbolSamples;
    data->ratio = data->framesForSymbols / (double)JS8_RX_SAMPLE_RATE;
    data->txDuration = data->ratio + data->startDelayMS/1000.0;
    data->toneDurationMS = (unsigned int)(data->ratio*1000/JS8_NUM_SYMBOLS);
}

// bit flags — these OR together, they are NOT exclusive states.
// e.g. a single-frame message is FIRST|LAST; a directed frame that's
// also the last frame of a multi-frame data message is LAST|DATA.
enum miniJS8_tx_type{
    MINIJS8_FRAME_NORMAL = 0,
    MINIJS8_FRAME_FIRST  = 1 << 0,
    MINIJS8_FRAME_LAST   = 1 << 1,
    MINIJS8_FRAME_DATA   = 1 << 2,   // fast-data submode marker (was JS8CallData)
};

typedef struct {
    uint8_t message[12]; //12 characters to encode into tones
    uint8_t flags;   // low 3 bits meaningful, see enum above
} miniJS8_Frame;

typedef struct {
    miniJS8_Frame *frames;
    size_t count;
} miniJS8_FrameList;

typedef struct {
    char dirCmd[16];
    char dirTo[16];
    char dirNum[8];
} miniJS8_MessageInfo;

typedef struct {
    miniJS8_Frame *data;
    size_t count;
    size_t cap;
} FrameBuf;

static void framebuf_push(FrameBuf *fb, miniJS8_Frame f) {
    if (fb->count == fb->cap) {
        fb->cap = fb->cap ? fb->cap * 2 : 4;
        fb->data = (miniJS8_Frame*)realloc(fb->data, fb->cap * sizeof(miniJS8_Frame));
    }
    fb->data[fb->count++] = f;
}

extern const char *basecalls[];

extern const char *cqs[];

static bool miniJS8_startsWithCQPrefix(const char *text) {
    for (size_t i = 0; i < 8; i++)
        if (strncmp(text, cqs[i], strlen(cqs[i])) == 0) return true;
    return false;
}

static uint8_t miniJS8_cqsReverseLookup(const char *type) {
    for (uint8_t i = 0; i < 8; i++)
        if (strcmp(cqs[i], type) == 0) return i;
    return 0; /* shouldn't happen given callers only pass a confirmed CQ-type string */
}

extern const int buffered_cmds[8];

typedef struct {
    int code;
    int size;
} checksum_cmd_t;

extern const checksum_cmd_t checksum_cmds[8];
/* directed_cmds: string -> command code. Exact-match lookup table,
 * mirroring the QMap<QString,int>. Order doesn't matter for lookup,
 * but kept in source order for readability/diffing against the original. */
typedef struct {
    const char *cmd;
    int code;
} DirectedCmdEntry;

extern const DirectedCmdEntry directed_cmds[38];

#define DIRECTED_CMDS_COUNT (sizeof(directed_cmds) / sizeof(directed_cmds[0]))

extern const int allowed_cmds[33];

typedef struct { char ch; uint16_t code; uint8_t len; } HuffEntry;

extern const HuffEntry hufftable[44];
#define HUFFTABLE_COUNT (sizeof(hufftable)/sizeof(hufftable[0]))

extern const char miniJS8_parityHexRows[87][23];
#define MINIJS8_PARITY_ROWS 87
#define MINIJS8_PARITY_COLS 87
#define MINIJS8_PARITY_WORD_BITS 64
#define MINIJS8_PARITY_TOTAL_BITS (MINIJS8_PARITY_ROWS * MINIJS8_PARITY_COLS)
#define MINIJS8_PARITY_WORD_COUNT ((MINIJS8_PARITY_TOTAL_BITS + MINIJS8_PARITY_WORD_BITS - 1) / MINIJS8_PARITY_WORD_BITS)


static uint64_t miniJS8_parityBits[MINIJS8_PARITY_WORD_COUNT];
static bool miniJS8_parityReady = false;

static uint8_t hex_nibble(char c) {
    if (c >= '0' && c <= '9') return (uint8_t)(c - '0');
    if (c >= 'a' && c <= 'f') return (uint8_t)(c - 'a' + 10);
    if (c >= 'A' && c <= 'F') return (uint8_t)(c - 'A' + 10);
    return 0; /* shouldn't happen -- table is fixed, valid hex */
}

/* Must be called once before miniJS8_parityBit() is used -- e.g. from a
 * one-time library init function, since C has no constexpr equivalent
 * to run this automatically at compile/load time. */
static void miniJS8_parityInit(void) {
    if (miniJS8_parityReady) return;

    memset(miniJS8_parityBits, 0, sizeof(miniJS8_parityBits));
    static const uint8_t masks[4] = {0x8, 0x4, 0x2, 0x1};

    for (size_t row = 0; row < MINIJS8_PARITY_ROWS; row++) {
        size_t col = 0;
        const char *rowStr = miniJS8_parityHexRows[row];

        for (size_t k = 0; rowStr[k] != '\0'; k++) {
            uint8_t value = hex_nibble(rowStr[k]);

            for (int m = 0; m < 4; m++) {
                if (col >= MINIJS8_PARITY_COLS) break; /* drop the 88th bit, same as original */
                if (value & masks[m]) {
                    size_t index = row * MINIJS8_PARITY_COLS + col;
                    miniJS8_parityBits[index / MINIJS8_PARITY_WORD_BITS] |=
                        ((uint64_t)1 << (index % MINIJS8_PARITY_WORD_BITS));
                }
                col++;
            }
        }
    }

    miniJS8_parityReady = true;
}

static inline int miniJS8_parityBit(size_t row, size_t col) {
    size_t index = row * MINIJS8_PARITY_COLS + col;
    return (int)((miniJS8_parityBits[index / MINIJS8_PARITY_WORD_BITS] >>
                  (index % MINIJS8_PARITY_WORD_BITS)) & 1);
}

#endif