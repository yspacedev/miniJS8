#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include "miniJS8/miniJS8.h"
#include "miniJS8/miniJS8_CRC.h"
#include "miniJS8/miniJS8_pack.h"
#include "miniJS8/miniJS8_parser.h"
//#include "miniJS8/miniJS8_const.h"

static int g_tests_run = 0;
static int g_tests_failed = 0;

#define CHECK(desc, cond) do { \
    g_tests_run++; \
    if (cond) { printf("  [PASS] %s\n", desc); } \
    else { printf("  [FAIL] %s\n", desc); g_tests_failed++; } \
} while (0)

static void print_frame_hex(const uint8_t msg[12]) {
    for (int i = 0; i < 12; i++) printf("%c", msg[i]);
}

int main(void) {
    printf("== miniJS8 smoke test ==\n\n");

    /* ---------------------------------------------------------
     * Callsign parsing / validation
     * --------------------------------------------------------- */
    printf("-- callsign parsing --\n");
    {
        bool isCompound;
        CHECK("KN4CRD is a valid callsign",
              miniJS8_isValidCallsign("KN4CRD", &isCompound) && !isCompound);

        CHECK("J1Y is a valid callsign (from buildFrames example)",
              miniJS8_isValidCallsign("J1Y", NULL));

        CHECK("HELLO is not a valid callsign (no digit)",
              !miniJS8_isValidCallsign("HELLO", NULL));

        CHECK("KN4CRD/VE3 is a compound callsign",
              miniJS8_isCompoundCallsign("KN4CRD/VE3"));

        CHECK("KN4CRD/P is NOT compound (portable suffix is part of base pattern)",
              !miniJS8_isCompoundCallsign("KN4CRD/P"));

        CHECK("@ALLCALL is a known basecall",
              miniJS8_isBasecall("@ALLCALL"));

        char calls[8][16];
        size_t n = miniJS8_parseCallsigns("J1Y ACK", calls, 8);
        CHECK("parseCallsigns('J1Y ACK') finds exactly 1 callsign",
              n == 1 && strcmp(calls[0], "J1Y") == 0);
    }
    printf("\n");

    /* ---------------------------------------------------------
     * Low-level packing
     * --------------------------------------------------------- */
    printf("-- packing primitives --\n");
    {
        bool portable = false;
        uint32_t packed = miniJS8_packCallsign("KN4CRD", &portable);
        CHECK("packCallsign('KN4CRD') is nonzero", packed != 0);
        printf("      packCallsign('KN4CRD') = %u\n", packed);

        uint32_t packedGroup = miniJS8_packCallsign("@ALLCALL", NULL);
        CHECK("packCallsign('@ALLCALL') matches its basecall code exactly",
              packedGroup == (uint32_t)(MINIJS8_NBASECALL + 2));

        uint16_t grid = miniJS8_packGrid("EM73");
        CHECK("packGrid('EM73') is not the 'no grid' sentinel "
              "(NOTE: grid2deg is an unconfirmed guess -- this only checks "
              "it produced *something*, not that the value is correct)",
              grid != (uint16_t)MINIJS8_NMAXGRID);
        printf("      packGrid('EM73') = %u\n", grid);

        uint16_t badGrid = miniJS8_packGrid("EM");
        CHECK("packGrid('EM') (too short) returns the sentinel",
              badGrid == (uint16_t)MINIJS8_NMAXGRID);
    }
    printf("\n");

    /* ---------------------------------------------------------
     * Checksums
     * --------------------------------------------------------- */
    printf("-- checksums --\n");
    {
        char c16a[4], c16b[4];
        miniJS8_checksum16("TEST", c16a);
        miniJS8_checksum16("TEST", c16b);
        CHECK("checksum16 output is exactly 3 chars", strlen(c16a) == 3);
        CHECK("checksum16 is deterministic (same input -> same output)",
              strcmp(c16a, c16b) == 0);
        printf("      checksum16('TEST') = \"%s\"\n", c16a);

        char c32[7];
        miniJS8_checksum32("TEST", c32);
        CHECK("checksum32 output is exactly 6 chars", strlen(c32) == 6);
        printf("      checksum32('TEST') = \"%s\"\n", c32);
    }
    printf("\n");

    /* ---------------------------------------------------------
     * Parity matrix
     * --------------------------------------------------------- */
    printf("-- parity matrix --\n");
    {
        miniJS8_parityInit();

        int first = miniJS8_parityBit(0, 0);
        int firstAgain = miniJS8_parityBit(0, 0);
        CHECK("parityBit is deterministic across repeated calls",
              first == firstAgain);

        int onesSeen = 0;
        for (size_t r = 0; r < 87; r++)
            for (size_t c = 0; c < 87; c++)
                if (miniJS8_parityBit(r, c)) onesSeen++;

        CHECK("parity matrix is not all-zero (init actually ran)",
              onesSeen > 0);
        printf("      %d of 7569 bits set\n", onesSeen);
    }
    printf("\n");

    /* ---------------------------------------------------------
     * buildFrames -- end to end
     * --------------------------------------------------------- */
    printf("-- buildFrames: heartbeat --\n");
    {
        miniJS8_FrameList list = miniJS8_buildFrames(
            "KN4CRD", "EM73", "", "CQ EM73",
            false, false, SUBMODE_NORMAL, NULL);

        CHECK("heartbeat message produces at least 1 frame", list.count >= 1);
        if (list.count > 0) {
            CHECK("first frame is flagged FIRST",
                  list.frames[0].flags & MINIJS8_FRAME_FIRST);
            CHECK("last frame is flagged LAST",
                  list.frames[list.count - 1].flags & MINIJS8_FRAME_LAST);

            printf("      %zu frame(s):\n", list.count);
            for (size_t i = 0; i < list.count; i++) {
                printf("        [%zu] flags=0x%02x msg=\"", i, list.frames[i].flags);
                print_frame_hex(list.frames[i].message);
                printf("\"\n");
            }
        }
        miniJS8_freeFrames(&list);
    }
    printf("\n");

    printf("-- buildFrames: directed message --\n");
    {
        miniJS8_MessageInfo info = {0};
        miniJS8_FrameList list = miniJS8_buildFrames(
            "KI5ZHW", "EL29", "", "J1Y ACK",
            false, false, SUBMODE_NORMAL, &info);

        CHECK("directed message produces at least 1 frame", list.count >= 1);
        CHECK("parsed 'to' field is J1Y", strcmp(info.dirTo, "J1Y") == 0);
        printf("      dirTo=\"%s\" dirCmd=\"%s\" dirNum=\"%s\"\n",
               info.dirTo, info.dirCmd, info.dirNum);

        for (size_t i = 0; i < list.count; i++) {
            printf("        [%zu] flags=0x%02x msg=\"", i, list.frames[i].flags);
            print_frame_hex(list.frames[i].message);
            printf("\"\n");
        }
        uint8_t out_tones[JS8_NUM_SYMBOLS];
        miniJS8_encode(&list.frames[0], &costas_arrays[COSTAS_ORIGINAL], out_tones);
        for (int i=0; i<7; i++){
            printf("tone %d = %d\r\n", i, out_tones[i]);
        }
        for (int i=72; i<79; i++){
            printf("tone %d = %d\r\n", i, out_tones[i]);
        }
        miniJS8_freeFrames(&list);
    }
    printf("\n");

    printf("-- buildFrames: forced data frame --\n");
    {
        miniJS8_FrameList list = miniJS8_buildFrames(
            "KN4CRD", "EM73", "", "HELLO",
            false, true, SUBMODE_NORMAL, NULL);

        CHECK("forced data message produces exactly 1 frame", list.count == 1);
        if (list.count == 1) {
            uint8_t flags = list.frames[0].flags;
            CHECK("that frame is flagged FIRST and LAST",
                  (flags & MINIJS8_FRAME_FIRST) && (flags & MINIJS8_FRAME_LAST));
            printf("      flags=0x%02x msg=\"", flags);
            print_frame_hex(list.frames[0].message);
            printf("\"\n");
        }
        miniJS8_freeFrames(&list);
    }
    printf("\n");

    /* ---------------------------------------------------------
     * Summary
     * --------------------------------------------------------- */
    printf("== %d/%d tests passed ==\n", g_tests_run - g_tests_failed, g_tests_run);
    return g_tests_failed == 0 ? 0 : 1;
}