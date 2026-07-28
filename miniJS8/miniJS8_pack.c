//frame-packing utilities

#include "miniJS8_pack.h"
#include "miniJS8_parser.h"

#define MASK(n) (((uint64_t)1 << (n)) - 1)

static void trim_inplace(char *s) {
    size_t len = strlen(s);
    size_t end = len;
    while (end > 0 && (s[end-1] == ' ' || s[end-1] == '\t')) end--;
    s[end] = '\0';
    size_t start = 0;
    while (s[start] == ' ' || s[start] == '\t') start++;
    if (start > 0) memmove(s, s + start, end - start + 1);
}

/* ============================================================
 * pack72bits
 * ============================================================ */
size_t miniJS8_pack72bits(uint64_t value, uint8_t rem, uint8_t outMsg[12]) {
    uint8_t mask4 = 0xF, mask6 = 0x3F;
    uint8_t remHigh = (uint8_t)(((value & mask4) << 2) | (rem >> 6));
    uint8_t remLow  = rem & mask6;
    value >>= 4;

    /* ASSUMPTION: stores the actual alphabet72 ASCII char, not the raw
     * 6-bit index -- matches what QString(packed,12) literally contained
     * in the original. Revisit once miniJS8_encode's real source is available;
     * if encode expects raw 0-63 indices this needs a one-line change. */
    outMsg[11] = (uint8_t)miniJS8_alphabet72[remLow];
    outMsg[10] = (uint8_t)miniJS8_alphabet72[remHigh];
    for (int i = 0; i < 10; i++) {
        outMsg[9-i] = (uint8_t)miniJS8_alphabet72[value & mask6];
        value >>= 6;
    }
    return 12;
}

/* ============================================================
 * packCallsign
 * ============================================================ */
static bool match_pack_callsign_pattern(const char s[6]) {
    bool ok0 = (is_upper_alpha(s[0]) || is_digit_c(s[0]) || s[0]==' ');
    bool ok1 = (is_upper_alpha(s[1]) || is_digit_c(s[1]));
    bool ok2 = is_digit_c(s[2]);
    bool ok3 = (is_upper_alpha(s[3]) || s[3]==' ');
    bool ok4 = (is_upper_alpha(s[4]) || s[4]==' ');
    bool ok5 = (is_upper_alpha(s[5]) || s[5]==' ');
    return ok0 && ok1 && ok2 && ok3 && ok4 && ok5;
}

uint32_t miniJS8_packCallsign(const char *value, bool *pPortable) {
    char callsign[16];
    size_t len = strlen(value);
    if (len >= sizeof(callsign)) len = sizeof(callsign) - 1;
    for (size_t i = 0; i < len; i++) callsign[i] = (char)toupper((unsigned char)value[i]);
    callsign[len] = '\0';
    trim_inplace(callsign);
    len = strlen(callsign);

    uint32_t basecallCode;
    if (miniJS8_basecallLookup(callsign, &basecallCode)) {
        return basecallCode;
    }

    if (len >= 2 && strcmp(callsign + len - 2, "/P") == 0) {
        callsign[len-2] = '\0';
        len -= 2;
        if (pPortable) *pPortable = true;
    }

    if (starts_with(callsign, "3DA0")) {
        char tmp[16];
        snprintf(tmp, sizeof(tmp), "3D0%s", callsign + 4);
        strcpy(callsign, tmp);
        len = strlen(callsign);
    }

    if (len > 2 && callsign[0]=='3' && callsign[1]=='X' && callsign[2] >= 'A' && callsign[2] <= 'Z') {
        char tmp[16];
        snprintf(tmp, sizeof(tmp), "Q%s", callsign + 2);
        strcpy(callsign, tmp);
        len = strlen(callsign);
    }

    if (len < 2 || len > 6) return 0;

    char perms[3][8];
    int nperms = 0;
    if (len == 6) {
        memcpy(perms[nperms++], callsign, 7);
    } else if (len == 2) {
        snprintf(perms[nperms++], 8, " %s   ", callsign);
    } else if (len == 3) {
        snprintf(perms[nperms++], 8, " %s  ", callsign);
        snprintf(perms[nperms++], 8, "%s   ", callsign);
    } else if (len == 4) {
        snprintf(perms[nperms++], 8, " %s ", callsign);
        snprintf(perms[nperms++], 8, "%s  ", callsign);
    } else { /* len == 5 */
        snprintf(perms[nperms++], 8, " %s", callsign);
        snprintf(perms[nperms++], 8, "%s ", callsign);
    }

    /* NOTE: original loops over ALL permutations and keeps the LAST match
     * (no break), so we must too -- not just take the first hit. */
    char matched[7] = {0};
    bool found = false;
    for (int i = 0; i < nperms; i++) {
        if (strlen(perms[i]) == 6 && match_pack_callsign_pattern(perms[i])) {
            memcpy(matched, perms[i], 6);
            matched[6] = '\0';
            found = true;
        }
    }
    if (!found) return 0;

    uint32_t packed;
    packed = (uint32_t)miniJS8_alphanumericIndex(matched[0]);
    packed = 36*packed + (uint32_t)miniJS8_alphanumericIndex(matched[1]);
    packed = 10*packed + (uint32_t)miniJS8_alphanumericIndex(matched[2]);
    packed = 27*packed + (uint32_t)miniJS8_alphanumericIndex(matched[3]) - 10;
    packed = 27*packed + (uint32_t)miniJS8_alphanumericIndex(matched[4]) - 10;
    packed = 27*packed + (uint32_t)miniJS8_alphanumericIndex(matched[5]) - 10;
    return packed;
}

/* ============================================================
 * packGrid
 * ============================================================ */

void miniJS8_grid2deg(const char *grid, int *outLong, int *outLat) {
    //grid can be either 4-character or 6-character.
    //for 4-character grids, fill in last characters with "mm"
    //though for our application, this really isn't needed
    char grid6[6];
    strcpy(grid6, grid);
    if (strlen(grid6)<6){
        grid6[4]='m';
        grid6[5]='m';
    }

    //NOTE: could probably do without float
    int nlong = 180 - 20 * (grid6[0] - 'A');
    int n20d  = 2 * (grid6[2] - '0');
    float xminlong  = 5 * (grid6[4] - 'a' + 0.5);
    float dlong = nlong - n20d - xminlong/60.0;
    *outLong = (int)dlong;

    int nlat = -90 + 10*(grid6[1] - 'A') + grid6[3] - '0';
    float xminlat = 2.5 * (grid6[5] - 'a' + 0.5);
    float dlat = nlat + xminlat/60.0;
    *outLat = (int)dlat;
}

uint16_t miniJS8_packGrid(const char *value) {
    char grid[8];
    strncpy(grid, value, 7);
    grid[7] = '\0';
    trim_inplace(grid);

    if (strlen(grid) < 4) return (uint16_t)MINIJS8_NMAXGRID;

    int ilong, ilat;
    miniJS8_grid2deg(grid, &ilong, &ilat);
    ilat += 90;
    return (uint16_t)(((ilong + 180) / 2) * 180 + ilat);
}

/* ============================================================
 * packNum
 * ============================================================ */
uint8_t miniJS8_packNum(const char *num, bool *ok) {
    if (num == NULL || num[0] == '\0') {
        if (ok) *ok = false;
        return 0;
    }
    char *endptr;
    long parsed = parser_strtol(num, &endptr, 10);
    if (ok) *ok = (endptr != num);
    if (parsed < -30) parsed = -30;
    if (parsed > 31) parsed = 31;
    return (uint8_t)(parsed + 30 + 1);
}

/* ============================================================
 * packCmd
 * ============================================================ */

/* ASSUMPTION -- Varicode::isSNRCommand not provided. Inferred: true only
 * for the two directed_cmds entries that actually carry "SNR" as a bare
 * (non-query) word: " SNR" (code 25) and " HEARTBEAT SNR" (code 29).
 * These are the only two codes for which directed_cmds_reverse is
 * unambiguous anyway (every other code has 1+ alias keys), so the
 * ambiguity in reverse lookup doesn't affect correctness here. */
static bool miniJS8_isSNRCommand(const char *cmdStr) {
    if (cmdStr == NULL) return false;
    return strcmp(cmdStr, " SNR") == 0 || strcmp(cmdStr, " HEARTBEAT SNR") == 0;
}

static const char *directed_cmds_reverse(int code) {
    /* first-match reverse lookup; safe because the only codes this is
     * meaningfully queried for (25, 29) are unique in the table */
    extern bool directed_cmds_lookup(const char*, int*); /* not used here directly */
    static const struct { const char *cmd; int code; } table[] = {
        {" HEARTBEAT",-1},{" HB",-1},{" CQ",-1},{" SNR?",0},{"?",0},
        {" DIT DIT",1},{" HEARING?",3},{" GRID?",4},{">",5},{" STATUS?",6},
        {" STATUS",7},{" HEARING",8},{" MSG",9},{" MSG TO:",10},{" QUERY",11},
        {" QUERY MSGS",12},{" QUERY MSGS?",12},{" QUERY CALL",13},{" GRID",15},
        {" INFO?",16},{" INFO",17},{" FB",18},{" HW CPY?",19},{" SK",20},
        {" RR",21},{" QSL?",22},{" QSL",23},{" CMD",24},{" SNR",25},{" NO",26},
        {" YES",27},{" 73",28},{" NACK",2},{" ACK",14},{" HEARTBEAT SNR",29},
        {" AGN?",30},{"  ",31},{" ",31},
    };
    for (size_t i = 0; i < sizeof(table)/sizeof(table[0]); i++)
        if (table[i].code == code) return table[i].cmd;
    return NULL;
}

uint8_t miniJS8_packCmd(uint8_t cmd, uint8_t num, bool *pPackedNum) {
    uint8_t value = 0;
    const char *cmdStr = directed_cmds_reverse(cmd);

    if (miniJS8_isSNRCommand(cmdStr)) {
        value = (uint8_t)(((1 << 1) | (cmdStr && strcmp(cmdStr, " HEARTBEAT SNR") == 0 ? 1 : 0)) << 6);
        value = (uint8_t)(value + (num & ((1 << 6) - 1)));
        if (pPackedNum) *pPackedNum = true;
    } else {
        value = cmd & ((1 << 7) - 1);
        if (pPackedNum) *pPackedNum = false;
    }
    return value;
}

/* ============================================================
 * packAlphaNumeric50
 * ============================================================ */
uint64_t miniJS8_packAlphaNumeric50(const char *value) {
    char word[16];
    size_t wlen = 0;
    for (const char *p = value; *p && wlen < sizeof(word)-1; p++) {
        char c = *p;
        if (is_upper_alpha(c) || is_digit_c(c) || c==' ' || c=='/' || c=='@') {
            word[wlen++] = c;
        }
    }
    word[wlen] = '\0';

    if (wlen > 3 && word[3] != '/' && wlen < sizeof(word)-1) {
        memmove(word+4, word+3, wlen-3+1);
        word[3] = ' ';
        wlen++;
    }
    if (wlen > 7 && word[7] != '/' && wlen < sizeof(word)-1) {
        memmove(word+8, word+7, wlen-7+1);
        word[7] = ' ';
        wlen++;
    }
    while (wlen < 11 && wlen < sizeof(word)-1) word[wlen++] = ' ';
    word[wlen] = '\0';

    uint64_t a = (uint64_t)38*38*38*2*38*38*38*2*38*38 * (uint64_t)miniJS8_alphanumericIndex(word[0]);
    uint64_t b = (uint64_t)38*38*38*2*38*38*38*2*38    * (uint64_t)miniJS8_alphanumericIndex(word[1]);
    uint64_t c = (uint64_t)38*38*38*2*38*38*38*2       * (uint64_t)miniJS8_alphanumericIndex(word[2]);
    uint64_t d = (uint64_t)38*38*38*2*38*38*38         * (word[3]=='/' ? 1 : 0);
    uint64_t e = (uint64_t)38*38*38*2*38*38            * (uint64_t)miniJS8_alphanumericIndex(word[4]);
    uint64_t f = (uint64_t)38*38*38*2*38               * (uint64_t)miniJS8_alphanumericIndex(word[5]);
    uint64_t g = (uint64_t)38*38*38*2                  * (uint64_t)miniJS8_alphanumericIndex(word[6]);
    uint64_t h = (uint64_t)38*38*38                    * (word[7]=='/' ? 1 : 0);
    uint64_t i = (uint64_t)38*38                       * (uint64_t)miniJS8_alphanumericIndex(word[8]);
    uint64_t j = (uint64_t)38                          * (uint64_t)miniJS8_alphanumericIndex(word[9]);
    uint64_t k =                                         (uint64_t)miniJS8_alphanumericIndex(word[10]);
    return a+b+c+d+e+f+g+h+i+j+k;
}

/* ============================================================
 * packCompoundFrame -- bitwise, fixed-width, no BitWriter needed here
 * ============================================================ */
size_t miniJS8_packCompoundFrame(const char *callsign, uint8_t type, uint16_t num, uint8_t bits3, uint8_t outMsg[12]) {
    if (type == MINIJS8_FRAME_TYPE_DATA || type == MINIJS8_FRAME_TYPE_DIRECTED) return 0;

    uint64_t packed_callsign = miniJS8_packAlphaNumeric50(callsign);
    if (packed_callsign == 0) return 0;

    uint16_t mask11 = ((uint16_t)0x7FF) << 5;
    uint8_t mask5 = 0x1F;
    uint16_t packed_11 = (uint16_t)((num & mask11) >> 5);
    uint8_t packed_5 = (uint8_t)(num & mask5);
    uint8_t packed_8 = (uint8_t)((packed_5 << 3) | (bits3 & 0x7));

    uint64_t value =
        (((uint64_t)(type & 0x7)) << 61) |
        ((packed_callsign & MASK(50)) << 11) |
        ((uint64_t)packed_11 & MASK(11));

    return miniJS8_pack72bits(value, packed_8, outMsg);
}

/* ============================================================
 * Regex-pattern parsers (callsign_pattern / optional_cmd_pattern /
 * optional_num_pattern / optional_grid_pattern), hand-rolled
 * ============================================================ */

static size_t match_callsign_prefix(const char *s, char out[16]) {
    size_t i = 0;
    if (s[0] == '@') i++;
    size_t atOffset = i;
    while (s[i] && (is_upper_alpha(s[i]) || is_digit_c(s[i]) || s[i]=='/')) i++;
    if (i == atOffset) return 0; /* the "+" part matched nothing */
    if (i >= 16) i = 15;
    memcpy(out, s, i);
    out[i] = '\0';
    return i;
}

static size_t match_cmd_alternation_at(const char *text) {
    static const struct { const char *word; char punct; } punctTokens[] = {
        {"AGN",'?'}, {"QSL",'?'}, {"HW CPY",'?'}, {"MSG TO",':'}, {"SNR",'?'},
        {"INFO",'?'}, {"GRID",'?'}, {"STATUS",'?'}, {"QUERY MSGS",'?'}, {"HEARING",'?'},
    };
    for (size_t i = 0; i < sizeof(punctTokens)/sizeof(punctTokens[0]); i++) {
        size_t wlen = strlen(punctTokens[i].word);
        if (strncmp(text, punctTokens[i].word, wlen) == 0 && text[wlen] == punctTokens[i].punct)
            return wlen + 1;
    }
    static const char *bareWords[] = {
        "STATUS","HEARING","QUERY CALL","QUERY MSGS","QUERY","CMD","MSG","NACK",
        "ACK","73","YES","NO","HEARTBEAT SNR","SNR","QSL","RR","SK","FB","INFO",
        "GRID","DIT DIT",
    };
    for (size_t i = 0; i < sizeof(bareWords)/sizeof(bareWords[0]); i++) {
        size_t wlen = strlen(bareWords[i]);
        if (strncmp(text, bareWords[i], wlen) == 0 && (text[wlen]==' ' || text[wlen]=='\0'))
            return wlen;
    }
    if (text[0]=='?' || text[0]=='>' || text[0]==' ') return 1;
    return 0;
}

/* \s? is greedy-by-default in PCRE: try consuming the leading space first;
 * only if that leads nowhere, backtrack to not consuming it (needed so a
 * lone trailing " " can still be caught by the [?> ] fallback). */
static size_t match_cmd_pattern(const char *s, char outCmd[16]) {
    size_t consumed = 0;
    if (s[0] == ' ') {
        size_t altLen = match_cmd_alternation_at(s + 1);
        if (altLen > 0) consumed = 1 + altLen;
    }
    if (consumed == 0) {
        size_t altLen = match_cmd_alternation_at(s);
        if (altLen > 0) consumed = altLen;
    }
    if (consumed == 0) { outCmd[0] = '\0'; return 0; }
    size_t copyLen = consumed < 15 ? consumed : 15;
    memcpy(outCmd, s, copyLen);
    outCmd[copyLen] = '\0';
    return consumed;
}

static size_t match_snr_digits(const char *s) {
    if (s[0]=='3' && (s[1]=='0' || s[1]=='1')) return 2;
    if (s[0]>='0' && s[0]<='2' && s[1]>='0' && s[1]<='9') return 2;
    if (s[0]>='0' && s[0]<='9') return 1;
    return 0;
}

/* includes the (?<=SNR) lookbehind: checks the 3 chars before matchStart
 * in the ORIGINAL line, not just within whatever cmd_pattern captured */
static size_t match_num_pattern(const char *lineStart, const char *matchStart, char outNum[8]) {
    size_t offset = (size_t)(matchStart - lineStart);
    if (offset < 3 || strncmp(matchStart - 3, "SNR", 3) != 0) {
        outNum[0] = '\0';
        return 0;
    }
    const char *p = matchStart;
    size_t consumed = 0;
    if (p[0] == ' ') consumed = 1;
    const char *afterSpace = p + consumed;
    size_t signLen = (afterSpace[0]=='-' || afterSpace[0]=='+') ? 1 : 0;
    size_t digitsLen = match_snr_digits(afterSpace + signLen);
    if (digitsLen == 0) { outNum[0] = '\0'; return 0; }
    consumed += signLen + digitsLen;
    size_t copyLen = consumed < 7 ? consumed : 7;
    memcpy(outNum, p, copyLen);
    outNum[copyLen] = '\0';
    return consumed;
}

static size_t match_directed_re(const char *text, char outCallsign[16], char outCmd[16], char outNum[8]) {
    size_t callsignLen = match_callsign_prefix(text, outCallsign);
    if (callsignLen == 0) { outCmd[0]='\0'; outNum[0]='\0'; return 0; }
    size_t cmdLen = match_cmd_pattern(text + callsignLen, outCmd);
    size_t numLen = match_num_pattern(text, text + callsignLen + cmdLen, outNum);
    return callsignLen + cmdLen + numLen;
}

static size_t match_compound_re(const char *text, char outCallsign[16], char outGrid[8], char outCmd[16], char outNum[8]) {
    size_t i = 0;
    while (text[i]==' ' || text[i]=='\t') i++;
    if (text[i] != '`') return 0;
    i++;

    size_t callsignLen = match_callsign_prefix(text + i, outCallsign);
    if (callsignLen == 0) return 0;
    i += callsignLen;

    outGrid[0] = '\0';
    {
        bool hasSpace = (text[i]==' ' || text[i]=='\t');
        const char *g = text + i + (hasSpace ? 1 : 0);
        if (g[0]>='A'&&g[0]<='R' && g[1]>='A'&&g[1]<='R' && g[2]>='0'&&g[2]<='9' && g[3]>='0'&&g[3]<='9') {
            memcpy(outGrid, g, 4);
            outGrid[4] = '\0';
            i += (hasSpace ? 1 : 0) + 4;
        }
    }

    i += match_cmd_pattern(text + i, outCmd);
    i += match_num_pattern(text, text + i, outNum);
    return i;
}

static const char *heartbeat_types[] = {
    "CQ CQ CQ","CQ DX","CQ QRP","CQ CONTEST","CQ FIELD","CQ FD","CQ CQ","CQ","HB","HEARTBEAT",
};

static size_t match_heartbeat_re(const char *text, char outType[16], char outGrid[8]) {
    size_t i = 0;
    while (text[i]==' ' || text[i]=='\t') i++;

    if (text[i] == '@') {
        size_t afterAt = i + 1;
        size_t wlen = 0;
        if (strncmp(text+afterAt, "ALLCALL", 7) == 0) wlen = 7;
        else if (strncmp(text+afterAt, "HB", 2) == 0) wlen = 2;
        if (wlen > 0) {
            size_t afterWord = afterAt + wlen;
            size_t sp = 0;
            while (text[afterWord+sp]==' ' || text[afterWord+sp]=='\t') sp++;
            if (sp > 0) i = afterWord + sp; /* only commit if \s+ actually matched */
        }
    }

    size_t typeLen = 0;
    const char *matchedType = NULL;
    for (size_t k = 0; k < sizeof(heartbeat_types)/sizeof(heartbeat_types[0]); k++) {
        size_t wlen = strlen(heartbeat_types[k]);
        if (strncmp(text+i, heartbeat_types[k], wlen) == 0) {
            if (strcmp(heartbeat_types[k], "HEARTBEAT") == 0) {
                size_t j = wlen, sp = 0;
                while (text[i+j+sp]==' ' || text[i+j+sp]=='\t') sp++;
                if (sp > 0 && strncmp(text+i+j+sp, "SNR", 3) == 0) continue; /* (?!\s+SNR) */
            }
            typeLen = wlen;
            matchedType = heartbeat_types[k];
            break;
        }
    }
    if (matchedType == NULL) return 0;
    strcpy(outType, matchedType);

    size_t afterType = i + typeLen;
    outGrid[0] = '\0';
    size_t gridLen = 0;
    if (text[afterType]==' ' || text[afterType]=='\t') {
        const char *g = text + afterType + 1;
        if (g[0]>='A'&&g[0]<='R' && g[1]>='A'&&g[1]<='R' && g[2]>='0'&&g[2]<='9' && g[3]>='0'&&g[3]<='9') {
            memcpy(outGrid, g, 4);
            outGrid[4] = '\0';
            gridLen = 1 + 4;
        }
    }
    return afterType + gridLen;
}

/* ============================================================
 * packHeartbeatMessage
 * ============================================================ */
size_t miniJS8_packHeartbeatMessage(const char *line, const char *mycall, uint8_t outMsg[12]) {
    char type[16], grid[8];
    size_t matchLen = match_heartbeat_re(line, type, grid);
    if (matchLen == 0) return 0;

    bool isAlt = starts_with(type, "CQ");
    if (mycall == NULL || mycall[0] == '\0') return 0;

    uint16_t packed_extra = (uint16_t)MINIJS8_NMAXGRID;
    if (strlen(grid) == 4) packed_extra = miniJS8_packGrid(grid);

    /* hbs.key(type,0): every hbs VALUE is literally "HB", so this reverse
     * lookup returns 0 whether type=="HB" (first/only match) or type is
     * something else (falls through to the default). It's always 0 --
     * simplified accordingly rather than porting a no-op map lookup. */
    uint8_t cqNumber = 0;
    if (isAlt) {
        packed_extra |= (1 << 15);
        cqNumber = miniJS8_cqsReverseLookup(type);
    }

    size_t frameLen = miniJS8_packCompoundFrame(mycall, MINIJS8_FRAME_TYPE_HEARTBEAT, packed_extra, cqNumber, outMsg);
    return frameLen == 0 ? 0 : matchLen; /* n only set on a successful frame -- matches original */
}

/* ============================================================
 * packCompoundMessage
 * ============================================================ */
size_t miniJS8_packCompoundMessage(const char *text, uint8_t outMsg[12]) {
    char callsign[16], grid[8], cmd[16], num[8];
    size_t matchLen = match_compound_re(text, callsign, grid, cmd, num);
    if (matchLen == 0) return 0;
    if (callsign[0] == '\0') return 0;

    uint8_t type = MINIJS8_FRAME_TYPE_COMPOUND;
    uint16_t extra = (uint16_t)MINIJS8_NMAXGRID;
    trim_inplace(num);

    int code;
    if (cmd[0] != '\0' && directed_cmds_lookup(cmd, &code) && miniJS8_isAllowedCode(code)) {
        bool packedNum = false;
        uint8_t inum = miniJS8_packNum(num, NULL);
        extra = (uint16_t)(MINIJS8_NUSERGRID + miniJS8_packCmd((uint8_t)code, inum, &packedNum));
        type = MINIJS8_FRAME_TYPE_COMPOUND_DIRECTED;
    } else if (grid[0] != '\0') {
        extra = miniJS8_packGrid(grid);
    }

    /* NOTE: unlike packHeartbeatMessage, the original sets *n unconditionally
     * here even if packCompoundFrame fails below -- preserved faithfully. */
    miniJS8_packCompoundFrame(callsign, type, extra, 0, outMsg);
    return matchLen;
}

/* ============================================================
 * packDirectedMessage
 * ============================================================ */
size_t miniJS8_packDirectedMessage(
    const char *text, const char *mycall,
    char dirTo[16], bool *dirToCompound,
    char dirCmd[16], char dirNum[8],
    uint8_t outMsg[12])
{
    char to[16], cmd[16], num[8];
    size_t matchLen = match_directed_re(text, to, cmd, num);
    if (matchLen == 0) return 0;

    char from[16];
    strncpy(from, mycall, 15); from[15] = '\0';
    /* isCompoundCallsign comes from minijs8_parse.c */
    extern bool miniJS8_isCompoundCallsign(const char *callsign);
    if (miniJS8_isCompoundCallsign(from)) strcpy(from, "<....>");

    if (cmd[0] == '\0') return 0;

    extern bool miniJS8_isValidCallsign(const char *callsign, bool *pIsCompound);
    bool isToCompound = false;
    bool validToCallsign = (strcmp(to, mycall) != 0) && miniJS8_isValidCallsign(to, &isToCompound);
    if (!validToCallsign) return 0;

    if (dirTo) { strncpy(dirTo, to, 15); dirTo[15] = '\0'; }
    if (dirToCompound) *dirToCompound = isToCompound;
    if (isToCompound) strcpy(to, "<....>");

    char cmdTrimmed[16];
    strcpy(cmdTrimmed, cmd);
    trim_inplace(cmdTrimmed);

    if (!miniJS8_isAllowedCommand(cmd) && !miniJS8_isAllowedCommand(cmdTrimmed)) return 0;

    char numTrimmed[8];
    strcpy(numTrimmed, num);
    trim_inplace(numTrimmed);
    bool numOK = false;
    uint8_t inum = miniJS8_packNum(numTrimmed, &numOK);
    if (numOK && dirNum) { strncpy(dirNum, num, 7); dirNum[7] = '\0'; }

    bool portable_from = false;
    uint32_t packed_from = miniJS8_packCallsign(from, &portable_from);
    bool portable_to = false;
    uint32_t packed_to = miniJS8_packCallsign(to, &portable_to);
    if (packed_from == 0 || packed_to == 0) return 0;

    char cmdOut[16] = {0};
    int packed_cmd = 0;
    int code;
    if (directed_cmds_lookup(cmd, &code)) { strcpy(cmdOut, cmd); packed_cmd = code; }
    if (directed_cmds_lookup(cmdTrimmed, &code)) { strcpy(cmdOut, cmdTrimmed); packed_cmd = code; } /* unconditionally re-checked, matches original */

    uint8_t packed_extra = (uint8_t)((((uint32_t)portable_from) << 7) | (((uint32_t)portable_to) << 6) | inum);

    uint64_t value =
        (((uint64_t)(MINIJS8_FRAME_TYPE_DIRECTED & 0x7)) << 61) |
        (((uint64_t)packed_from & MASK(28)) << 33) |
        (((uint64_t)packed_to   & MASK(28)) << 5)  |
        ((uint64_t)((uint32_t)packed_cmd % 32) & MASK(5));

    if (dirCmd) { strncpy(dirCmd, cmdOut, 15); dirCmd[15] = '\0'; }

    miniJS8_pack72bits(value, packed_extra, outMsg);
    return matchLen;
}

/* ============================================================
 * BitWriter -- byte-buffer + shift/OR, replaces QVector<bool> entirely
 * ============================================================ */
#define MINIJS8_FRAME_BITS 72

typedef struct {
    uint8_t buf[(MINIJS8_FRAME_BITS + 7) / 8];
    size_t bitCount;
} BitWriter;

static void bitwriter_init(BitWriter *bw) {
    memset(bw->buf, 0, sizeof(bw->buf));
    bw->bitCount = 0;
}

static void bitwriter_write(BitWriter *bw, uint32_t value, size_t nbits) {
    for (size_t i = 0; i < nbits; i++) {
        size_t bitIndex = bw->bitCount + i;
        size_t byteIndex = bitIndex / 8;
        size_t bitOffset = 7 - (bitIndex % 8);
        uint8_t bit = (uint8_t)((value >> (nbits - 1 - i)) & 1);
        bw->buf[byteIndex] |= (uint8_t)(bit << bitOffset);
    }
    bw->bitCount += nbits;
}

static void bitwriter_pad(BitWriter *bw) {
    bool first = true;
    while (bw->bitCount < MINIJS8_FRAME_BITS) {
        bitwriter_write(bw, first ? 0 : 1, 1);
        first = false;
    }
}

static void bitwriter_finalize(const BitWriter *bw, uint64_t *value, uint8_t *rem) {
    uint64_t v = 0;
    for (size_t i = 0; i < 64; i++) {
        uint8_t bit = (bw->buf[i/8] >> (7 - (i%8))) & 1;
        v = (v << 1) | bit;
    }
    uint8_t r = 0;
    for (size_t i = 64; i < 72; i++) {
        uint8_t bit = (bw->buf[i/8] >> (7 - (i%8))) & 1;
        r = (uint8_t)((r << 1) | bit);
    }
    *value = v; *rem = r;
}

/* ============================================================
 * packHuffMessage -- single-pass fusion of the original's huffEncode()
 * + packHuffMessage() loop (equivalent behavior, no intermediate list)
 * ============================================================ */
size_t miniJS8_packHuffMessage(const char *input, uint8_t prefixBits, size_t prefixCount, uint8_t outMsg[12]) {
    /* NOTE -- preserved quirk: original validates the UPPERCASED char but
     * then encodes using the ORIGINAL (possibly lowercase) char against an
     * uppercase-only table, so lowercase letters pass validation here but
     * then silently contribute 0 bits in the loop below (treated as
     * "no match, skip"). This looks like a bug upstream; ported as-is.
     * Confirm whether you want it preserved or fixed. */
    for (const char *p = input; *p; p++) {
        if (!miniJS8_huffIsValidUpper(*p)) return 0;
    }

    BitWriter bw;
    bitwriter_init(&bw);
    if (prefixCount > 0) bitwriter_write(&bw, prefixBits, prefixCount);

    size_t charsConsumed = 0;
    const char *p = input;
    while (*p) {
        uint16_t code; uint8_t len;
        if (!miniJS8_huffLookup(*p, &code, &len)) { p++; continue; }
        if (bw.bitCount + len >= MINIJS8_FRAME_BITS) break; /* strict <, leaves room for pad terminator */
        bitwriter_write(&bw, code, len);
        charsConsumed++;
        p++;
    }

    bitwriter_pad(&bw);
    uint64_t value; uint8_t rem;
    bitwriter_finalize(&bw, &value, &rem);
    miniJS8_pack72bits(value, rem, outMsg);
    return charsConsumed;
}

/* STUB -- compression deferred. Structure mirrors packHuffMessage so a real
 * implementation (JSC::compress port) can drop in later without touching
 * packDataMessage/packFastDataMessage. */
size_t miniJS8_packCompressedMessage(const char *input, uint8_t prefixBits, size_t prefixCount, uint8_t outMsg[12]) {
    (void)input; (void)prefixBits; (void)prefixCount;
    memset(outMsg, 0, 12);
    return 0;
}

/* ============================================================
 * packDataMessage / packFastDataMessage
 * ============================================================ */
size_t miniJS8_packDataMessage(const char *input, uint8_t outMsg[12]) {
    uint8_t huffMsg[12];
    size_t huffChars = miniJS8_packHuffMessage(input, 0x2 /*0b10*/, 2, huffMsg);
    uint8_t compMsg[12];
    size_t compChars = miniJS8_packCompressedMessage(input, 0x3 /*0b11*/, 2, compMsg);

    if (huffChars > compChars) { memcpy(outMsg, huffMsg, 12); return huffChars; }
    memcpy(outMsg, compMsg, 12);
    return compChars;
}

/* ASSUMPTION: JS8_FAST_DATA_CAN_USE_HUFF's real default is unknown (like
 * JS8_NO_MULTILINE earlier, this is an external build flag never shown to
 * us). Defaulting to 1 (fast mode supports huff) since that's the more
 * feature-complete branch given in your source. Flip to 0 if fast-mode
 * frames should only ever use compression. */
#define JS8_FAST_DATA_CAN_USE_HUFF 1

size_t miniJS8_packFastDataMessage(const char *input, uint8_t outMsg[12]) {
#if JS8_FAST_DATA_CAN_USE_HUFF
    uint8_t huffMsg[12];
    size_t huffChars = miniJS8_packHuffMessage(input, 0, 1, huffMsg);
    uint8_t compMsg[12];
    size_t compChars = miniJS8_packCompressedMessage(input, 1, 1, compMsg);
    if (huffChars > compChars) { memcpy(outMsg, huffMsg, 12); return huffChars; }
    memcpy(outMsg, compMsg, 12);
    return compChars;
#else
    return miniJS8_packCompressedMessage(input, 0, 0, outMsg);
#endif
}