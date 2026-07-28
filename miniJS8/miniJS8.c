#include "miniJS8.h"
#include "miniJS8_CRC.h"
#include "miniJS8_const.h"
#include "miniJS8_pack.h"
#include "miniJS8_parser.h"


/* ============================================================
 * miniJS8_buildFrames — C port of Varicode::buildMessageFrames
 * ============================================================ */

miniJS8_FrameList miniJS8_buildFrames(
    const char *mycall,
    const char *mygrid,
    const char *selectedCall,
    const char *text,
    bool forceIdentify,
    bool forceData,
    int submode,
    miniJS8_MessageInfo *pInfo)
{
    /* feature toggles — same intent as the #define block in the original */
    const bool ALLOW_SEND_COMPOUND                     = true;
    const bool ALLOW_SEND_COMPOUND_DIRECTED             = true;
    const bool AUTO_PREPEND_DIRECTED                    = true;
    const bool AUTO_REMOVE_MYCALL                       = true;
    const bool AUTO_PREPEND_DIRECTED_ALLOW_TEXT_CALLSIGNS = true;
    const bool ALLOW_FORCE_IDENTIFY                     = true;
    const bool AUTO_RSTRIP_WHITESPACE                   = true;

    bool mycallCompound = miniJS8_isCompoundCallsign(mycall);

    FrameBuf fb = {0};

    /* NOTE: unlike the original QStringList lines = { text }, we skip the
     * dead multiline-split branch entirely and just treat `text` as the
     * single line to process, JS8_NO_MULTILINE
     * was never defined so that branch never ran anyway. */

    char line[MINIJS8_LINE_BUFSIZE];
    if (strlen(text) >= sizeof(line)) {
        /* caller's message too long for our fixed buffer; truncate
         * defensively rather than overflow. Consider raising
         * MINIJS8_LINE_BUFSIZE if this matters for your use case. */
        memcpy(line, text, sizeof(line) - 1);
        line[sizeof(line) - 1] = '\0';
    } else {
        strcpy(line, text);
    }

    bool hasDirected = false;
    bool hasData = false;

    if (forceData) {
        forceIdentify = false;
        hasData = true;
    }

    /* AUTO_REMOVE_MYCALL: strip "MYCALL:" or "MYCALL " from the front */
    if (AUTO_REMOVE_MYCALL) {
        size_t mycallLen = strlen(mycall);
        char withColon[24], withSpace[24];
        snprintf(withColon, sizeof(withColon), "%s:", mycall);
        snprintf(withSpace, sizeof(withSpace), "%s ", mycall);

        if (starts_with(line, withColon) || starts_with(line, withSpace)) {
            buf_consume(line, mycallLen + 1);
            buf_lstrip(line);
        }
    }

    if (AUTO_RSTRIP_WHITESPACE) {
        buf_rstrip_if_nonempty(line);
    }

    /* AUTO_PREPEND_DIRECTED: prefix selectedCall onto the line unless it's
     * already directed at someone, a raw message, or forced data */
    if (AUTO_PREPEND_DIRECTED &&
        selectedCall != NULL && selectedCall[0] != '\0' &&
        !starts_with(line, selectedCall) &&
        !starts_with(line, "`") &&
        !forceData)
    {
        bool lineStartsWithBaseCall =
            starts_with(line, "@ALLCALL") ||
            miniJS8_startsWithCQ(line) ||
            miniJS8_startsWithHB(line);

        bool lineStartsWithStandardCall = false;
        if (AUTO_PREPEND_DIRECTED_ALLOW_TEXT_CALLSIGNS) {
            char calls[MINIJS8_MAX_CALLSIGNS][16];
            size_t nCalls = miniJS8_parseCallsigns(line, calls, MINIJS8_MAX_CALLSIGNS);
            if (nCalls > 0 &&
                starts_with(line, calls[0]) &&
                strlen(calls[0]) > 3)
            {
                lineStartsWithStandardCall = true;
            }
        }

        if (!(lineStartsWithBaseCall || lineStartsWithStandardCall)) {
            char prefix[24];
            const char *sep = (line[0] == ' ') ? "" : " ";
            snprintf(prefix, sizeof(prefix), "%s%s", selectedCall, sep);
            buf_prepend(line, sizeof(line), prefix);
        }
    }

    /* main per-message framing loop — mirrors the C++ while(line.size() > 0) */
    while (line[0] != '\0') {
        uint8_t frame[12];
        int frameFlags = MINIJS8_FRAME_NORMAL;

        bool useBcn = false;
        bool useCmp = false;
        bool useDir = false;
        bool useDat = false;

        uint8_t bcnFrame[12];
        size_t l = miniJS8_packHeartbeatMessage(line, mycall, bcnFrame);

        uint8_t cmpFrame[12];
        size_t o = 0;
        if (ALLOW_SEND_COMPOUND) {
            o = miniJS8_packCompoundMessage(line, cmpFrame);
        }

        char dirCmd[8] = {0};
        char dirTo[16] = {0};
        char dirNum[8] = {0};
        bool dirToCompound = false;
        uint8_t dirFrame[12];
        size_t n = miniJS8_packDirectedMessage(
            line, mycall, dirTo, &dirToCompound, dirCmd, dirNum, dirFrame);

        /* ALLOW_FORCE_IDENTIFY: auto-prepend mycall if this looks like a
         * bare data frame and forceIdentify was requested */
        bool isLikelyDataFrame =
            (fb.count == 0) &&
            (selectedCall == NULL || selectedCall[0] == '\0') &&
            (dirTo[0] == '\0') &&
            (l == 0) && (o == 0);

        if (ALLOW_FORCE_IDENTIFY && forceIdentify && isLikelyDataFrame &&
            strstr(line, mycall) == NULL)
        {
            char prefix[24];
            snprintf(prefix, sizeof(prefix), "%s: ", mycall);
            buf_prepend(line, sizeof(line), prefix);
        }

        uint8_t datFrame[12];
        bool fastDataFrame;
        size_t m;
        if (submode == SUBMODE_NORMAL) {
            m = miniJS8_packDataMessage(line, datFrame);
            fastDataFrame = false;
        } else {
            m = miniJS8_packFastDataMessage(line, datFrame);
            fastDataFrame = true;
        }

        /* priority: heartbeat > compound > directed > data, but only
         * heartbeat/compound/directed are allowed before the first
         * directed/data frame has been sent for this message */
        if (!hasDirected && !hasData && l > 0) {
            useBcn = true;
            hasDirected = false;
            memcpy(frame, bcnFrame, 12);
        } else if (ALLOW_SEND_COMPOUND && !hasDirected && !hasData && o > 0) {
            useCmp = true;
            hasDirected = false;
            memcpy(frame, cmpFrame, 12);
        } else if (!hasDirected && !hasData && n > 0) {
            useDir = true;
            hasDirected = true;
            memcpy(frame, dirFrame, 12);
        } else if (m > 0) {
            useDat = true;
            hasData = true;
            memcpy(frame, datFrame, 12);
        } else {
            /* nothing matched and there's still text left — avoid an
             * infinite loop. This shouldn't happen if packDataMessage
             * always consumes at least 1 char when line is non-empty. */
            break;
        }

        if (useBcn) {
            miniJS8_Frame f;
            memcpy(f.message, frame, 12);
            f.flags = frameFlags;
            framebuf_push(&fb, f);
            buf_consume(line, l);
        }

        if (ALLOW_SEND_COMPOUND && useCmp) {
            miniJS8_Frame f;
            memcpy(f.message, frame, 12);
            f.flags = frameFlags;
            framebuf_push(&fb, f);
            buf_consume(line, o);
        }

        if (useDir) {
            bool shouldUseStandardFrame = true;

            if (ALLOW_SEND_COMPOUND_DIRECTED && (mycallCompound || dirToCompound)) {
                /* CASE 1/2/3 from the original comment block:
                 * send a standard compound frame for our own call+grid,
                 * followed by a (possibly compound) directed frame */
                char deCompoundMsg[40];
                snprintf(deCompoundMsg, sizeof(deCompoundMsg), "`%s %s", mycall, mygrid);
                uint8_t deCompoundFrame[12];
                size_t deLen = miniJS8_packCompoundMessage(deCompoundMsg, deCompoundFrame);
                if (deLen > 0) {
                    miniJS8_Frame f;
                    memcpy(f.message, deCompoundFrame, 12);
                    f.flags = MINIJS8_FRAME_NORMAL;
                    framebuf_push(&fb, f);
                }

                char dirCompoundMsg[40];
                snprintf(dirCompoundMsg, sizeof(dirCompoundMsg), "`%s%s%s", dirTo, dirCmd, dirNum);
                uint8_t dirCompoundFrame[12];
                size_t dirLen = miniJS8_packCompoundMessage(dirCompoundMsg, dirCompoundFrame);
                if (dirLen > 0) {
                    miniJS8_Frame f;
                    memcpy(f.message, dirCompoundFrame, 12);
                    f.flags = MINIJS8_FRAME_NORMAL;
                    framebuf_push(&fb, f);
                }

                shouldUseStandardFrame = false;
            }

            if (shouldUseStandardFrame) {
                miniJS8_Frame f;
                memcpy(f.message, frame, 12);
                f.flags = frameFlags;
                framebuf_push(&fb, f);
            }

            buf_consume(line, n);

            /* buffered command checksum handling */
            if (miniJS8_isCommandBuffered(dirCmd) && line[0] != '\0') {
                buf_lstrip(line);

                int checksumSize = miniJS8_isCommandChecksumed(dirCmd);
                if (checksumSize == 32) {
                    char cksum[9];
                    miniJS8_checksum32(line, cksum);
                    buf_append(line, sizeof(line), " ");
                    buf_append(line, sizeof(line), cksum);
                } else if (checksumSize == 16) {
                    char cksum[5];
                    miniJS8_checksum16(line, cksum);
                    buf_append(line, sizeof(line), " ");
                    buf_append(line, sizeof(line), cksum);
                }
                /* checksumSize == 0: no checksum required */
            }

            if (pInfo) {
                strncpy(pInfo->dirCmd, dirCmd, sizeof(pInfo->dirCmd) - 1);
                strncpy(pInfo->dirTo, dirTo, sizeof(pInfo->dirTo) - 1);
                strncpy(pInfo->dirNum, dirNum, sizeof(pInfo->dirNum) - 1);
            }
        }

        if (useDat) {
            miniJS8_Frame f;
            memcpy(f.message, frame, 12);
            f.flags = fastDataFrame ? MINIJS8_FRAME_DATA : MINIJS8_FRAME_NORMAL;
            framebuf_push(&fb, f);
            buf_consume(line, m);
        }
    }

    if (fb.count > 0) {
        fb.data[0].flags |= MINIJS8_FRAME_FIRST;
        fb.data[fb.count - 1].flags |= MINIJS8_FRAME_LAST;
    }

    /* shrink-wrap before returning */
    if (fb.count > 0 && fb.count < fb.cap) {
        miniJS8_Frame *shrunk = (miniJS8_Frame *)realloc(fb.data, fb.count * sizeof(miniJS8_Frame));
        if (shrunk) fb.data = shrunk;
    }

    miniJS8_FrameList result;
    result.frames = fb.data;
    result.count = fb.count;
    return result;
}

void miniJS8_freeFrames(miniJS8_FrameList *list) {
    free(list->frames);
    list->frames = NULL;
    list->count = 0;
}

uint8_t alphabet_lut[255];

void init_alphabet_lut(){
    for (int i=0; i<255; i++){
        alphabet_lut[i]=255; //invalid
    }
    for (int i=0; i<strlen(miniJS8_alphabet); i++){ //not sure about strlen here
        alphabet_lut[miniJS8_alphabet[i]]=i;
    }
}

uint8_t alphabet_lookup(uint8_t c){
    uint8_t ret = alphabet_lut[c];
    if (ret==255){
        printf("Invald character\r\n");
    }
    return ret;
}

void miniJS8_encode(miniJS8_Frame *frame, const miniJS8_costas_t *costas, uint8_t* tones){
    // Our initial goal here is an 87-bit message, create a 87/8 = 10.875 so we need an 11-byte array
    //
    // Message structure:
    //
    //     +----------+----------+----------+
    //     |          |          |  72 bits |  12 6-bit words
    //     |          |          +==========+
    //     |          | 87 bits  |   3 bits |  Frame type
    //     | 11 bytes |          +==========+
    //     |          |          |  12 bits |  12-bit BE checksum
    //     |          |----------+==========+
    //     |          |  1 bit   |   1 bit  |  Leftover bit in array
    //     +----------+----------+==========+

    uint8_t bytes[11];
    uint8_t *msg = frame->message;

    //convert the 12 characters in frame->message to 6-bit words
    //and pack into byte array
    for (int i = 0, j = 0; i < 12; i += 4, j += 3){
        uint32_t words = (alphabet_lookup(msg[i  ]) << 18) |
                         (alphabet_lookup(msg[i+1]) << 12) |
                         (alphabet_lookup(msg[i+2]) <<  6) |
                          alphabet_lookup(msg[i+3]);

        bytes[j    ] = words >> 16;
        bytes[j + 1] = words >>  8;
        bytes[j + 2] = words;
    }

    //pack message type - 3 bit flag used to determine whether a transmission is a start, middle, end, or data
    bytes[9] = (frame->flags & 0b111) << 5;

    //now compute crc and use in last fields of bytes
    uint16_t crc = miniJS8_CRC12(bytes, 11);

    //pack into last 12 bits of 87 bit payload

    bytes[9] |= (crc >> 7) & 0x1F;
    bytes[10] = (crc & 0x7F) << 1;

    // That's it for our 87-bit message; we're now going to turn it
    // into two blocks of 29 3-bit words, which will in turn become
    // tones, the first block being parity for the second, bracketed
    // by the Costas arrays.
    //
    // Output structure:
    //
    //     +----------+----------+
    //     |          |  7 bytes |  Costas array A
    //     |          +==========+
    //     |          | 29 bytes |  Parity data
    //     |          +==========+
    //     | 79 bytes |  7 bytes |  Costas array B
    //     |          +==========+
    //     |          | 29 bytes |  Output data
    //     |          +==========+
    //     |          |  7 bytes |  Costas array C
    //     +----------+==========+

    uint8_t *costasData = tones;
    uint8_t *parityData = tones + 7;
    uint8_t *outputData = tones + 43;

    //output costas arrays in correct positions - at idx 0, 36, 72
    for (int costas_idx=0; costas_idx<3; costas_idx++){
        //copy array into costasData
        for (int i=0; i<7; i++){
            costasData[i]=costas->x[costas_idx][i];
        }
        costasData+=36; //change position next array gets put in
    }

    // Our 87 data bits are going to be morphed into two sets of 29 3-bit
    // words, the first one parity for the second; we're going to do
    // this in parallel.

    size_t  outputBits  = 0;
    size_t  outputByte  = 0;
    uint8_t outputMask  = 0x80;
    uint8_t outputWord  = 0;
    uint8_t parityWord  = 0;
        
    for (size_t i = 0; i < 87; ++i){
        // Compute parity for the current bit; inputs for parity computation
        // are the corresponding parity matrix row and each bit in the message;
        // the parity matrix row, referenced by `i`, contains 87 boolean values.
        // Each `true` value defines a message bit that must be summed, modulo
        // 2, to produce the parity check bit for the bit we're working on now.
        //
        // In short, if the parity matrix bit `(i, j)` and the message bit `j`
        // are both set, then we add 1 to the parity bits accumulator. If, after
        // processing all message bits the accumulated result is odd, then the
        // parity bit should be set for the current bit.

        size_t  parityBits = 0;
        size_t  parityByte = 0;
        uint8_t parityMask = 0x80;
        
        for (size_t j = 0; j < 87; ++j)
        {
            parityBits += miniJS8_parityBit(i, j) && (bytes[parityByte] & parityMask);
            parityMask  = (parityMask == 1) ? (++parityByte, 0x80) : (parityMask >> 1);
        }
        
        // Accumulate the parity and output bits; this is the point at which
        // we perform the modulo 2 operation on the summed parity bits.

        parityWord = (parityWord << 1) | (parityBits & 1);
        outputWord = (outputWord << 1) | ((bytes[outputByte] & outputMask) != 0);
        outputMask = (outputMask == 1) ? (++outputByte, 0x80) : (outputMask >> 1);
        
        // If we're at a 3-bit boundary, output the words and reset.

        if (++outputBits == 3)
        {
            *parityData++ = parityWord;
            *outputData++ = outputWord;
            parityWord    = 0;
            outputWord    = 0;
            outputBits    = 0;
        }
    }
}

//since C lacks compile time execuation, we have to build some arrays and fields
void miniJS8_init(){
    init_alphabet_lut(); //look-up table for converting charaters to 6-bit
    miniJS8_parityInit();
    for (int i = 0; i<(sizeof(miniJS8_submodes)/sizeof(miniJS8_data_t)); i++){
        miniJS8_compute_submode((miniJS8_data_t*)&miniJS8_submodes[i]);
    }
}