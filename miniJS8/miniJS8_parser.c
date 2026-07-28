//parsing utilities

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <malloc.h>
#include <string.h>
#include "miniJS8_parser.h"

/* ============================================================
 * Small buffer helpers replacing QString::mid / prepend / append
 * ============================================================ */

 size_t count_char(const char *s, char c) {
    size_t count = 0;
    for (; *s; s++) {
        if (*s == c) count++;
    }
    return count;
}

static bool is_grid_block(const char *s, size_t len, bool has_subsquare) {
    if (len < 4) return false;
    if (s[0] < 'A' || s[0] > 'X') return false;
    if (s[1] < 'A' || s[1] > 'X') return false;
    if (!is_digit_c((unsigned char)s[2]) || !is_digit_c((unsigned char)s[3])) return false;
    return true;
}


/* strip leading whitespace in place, return pointer to first non-space char
 * (mirrors Varicode::lstrip, but done via memmove so the buffer stays
 * self-contained rather than returning an offset pointer) */
 void buf_lstrip(char *buf) {
    size_t i = 0;
    while (buf[i] == ' ' || buf[i] == '\t') i++;
    if (i > 0) memmove(buf, buf + i, strlen(buf) - i + 1);
}

/* strip trailing whitespace in place, but only if the result is non-empty
 * (mirrors the AUTO_RSTRIP_WHITESPACE guard: "as long as there are
 * characters left afterwards") */
void buf_rstrip_if_nonempty(char *buf) {
    size_t len = strlen(buf);
    size_t end = len;
    while (end > 0 && (buf[end - 1] == ' ' || buf[end - 1] == '\t')) end--;
    if (end > 0) buf[end] = '\0';
}

/* drop the first n chars, shifting the remainder (incl. null terminator) left */
void buf_consume(char *buf, size_t n) {
    size_t len = strlen(buf);
    if (n > len) n = len;
    memmove(buf, buf + n, len - n + 1);
}

/* insert prefix at the very front of buf; returns false if it wouldn't fit */
bool buf_prepend(char *buf, size_t bufsize, const char *prefix) {
    size_t plen = strlen(prefix);
    size_t blen = strlen(buf);
    if (plen + blen + 1 > bufsize) return false;
    memmove(buf + plen, buf, blen + 1);
    memcpy(buf, prefix, plen);
    return true;
}

/* append suffix to the end of buf; returns false if it wouldn't fit */
bool buf_append(char *buf, size_t bufsize, const char *suffix) {
    size_t blen = strlen(buf);
    size_t slen = strlen(suffix);
    if (blen + slen + 1 > bufsize) return false;
    memcpy(buf + blen, suffix, slen + 1);
    return true;
}

bool starts_with(const char *s, const char *prefix) {
    return strncmp(s, prefix, strlen(prefix)) == 0;
}

bool is_upper_alpha(char c) { return c >= 'A' && c <= 'Z'; }
bool is_digit_c(char c)     { return c >= '0' && c <= '9'; }
bool is_alnum_call(char c)  { return is_upper_alpha(c) || is_digit_c(c); }

static bool is_call_char(char c) {
    return is_upper_alpha(c) || is_digit_c(c) || c == '/' || c == '@';
}

/* Mimics a regex \b at position i: true if the previous character
 * (if any) is NOT a call_char — i.e. we're at a transition into a
 * call-char run, or at the very start of the string. */
static bool at_boundary(const char *s, size_t i) {
    if (i == 0) return true;
    return !is_call_char(s[i - 1]);
}

/* Varicode::parseCallsigns — regex tokenizer + isValidCallsign + grid filter.
 * out must point to an array of MINIJS8_MAX_CALLSIGNS buffers, each >=16 bytes.
 * Returns number of callsigns found. */
// Scans `input` for candidate tokens, validates each, excludes grids,
// writes results into out[] (caller-allocated array of char[16] or similar),
// returns count found (capped at max_out).
size_t miniJS8_parseCallsigns(const char *input, char out[][16], size_t max_out) {
    size_t n = strlen(input);
    size_t found = 0;
    size_t i = 0;

    while (i < n && found < max_out) {
        if (!is_call_char(input[i]) || !at_boundary(input, i)) {
            i++;
            continue;
        }

        // greedily consume the run of call_chars (regex's bounded groups
        // just cap max token length in practice — bound it similarly)
        size_t start = i;
        size_t len = 0;
        while (i < n && is_call_char(input[i]) && len < 13) {
            i++;
            len++;
        }

        // must end on a boundary (next char is not a call_char, or end of string)
        if (i < n && is_call_char(input[i])) {
            // ran into max length mid-token; not a clean match, skip past it
            while (i < n && is_call_char(input[i])) i++;
            continue;
        }

        if (len == 0 || len >= 16) continue;

        char token[16];
        bool compound = false;
        memcpy(token, input + start, len);
        token[len] = '\0';

        if (!miniJS8_isValidCallsign(token, &compound) ) continue;
        if (miniJS8_isGridLocator(token))     continue;

        memcpy(out[found], token, len + 1);
        found++;
    }

    return found;
}

/* Exact port of base_callsign_pattern, tested for full-string match:
 *   \b (alnum)? (alnum) (digit) (letter){0,3} (/P)? \b
 * Tries both parses of the optional leading char, since a hand-rolled
 * scanner can't backtrack the way PCRE does. */
bool match_base_callsign_full(const char *s) {
    size_t n = strlen(s);
    if (n == 0) return false;

    size_t baseLen = n;
    if (n >= 2 && s[n - 2] == '/' && s[n - 1] == 'P') {
        baseLen = n - 2;
    }
    if (baseLen < 2 || baseLen > 6) return false;

    for (int leadPresent = 0; leadPresent <= 1; leadPresent++) {
        size_t idx = 0;
        if (leadPresent) {
            if (baseLen < 3) continue;
            if (!is_alnum_call(s[idx])) continue;
            idx++;
        }
        if (idx >= baseLen || !is_alnum_call(s[idx])) continue;
        idx++;
        if (idx >= baseLen || !is_digit_c(s[idx])) continue;
        idx++;

        size_t lettersLen = baseLen - idx;
        if (lettersLen > 3) continue;

        bool allLetters = true;
        for (size_t k = idx; k < baseLen; k++) {
            if (!is_upper_alpha(s[k])) { allLetters = false; break; }
        }
        if (allLetters) return true;
    }
    return false;
}

/* Matches the extra check in isValidCallsign: "[0-9][A-Z]|[A-Z][0-9]"
 * anywhere in the string (an adjacent digit/letter transition) */
bool has_digit_letter_transition(const char *s) {
    size_t n = strlen(s);
    for (size_t i = 0; i + 1 < n; i++) {
        if ((is_digit_c(s[i]) && is_upper_alpha(s[i + 1])) ||
            (is_upper_alpha(s[i]) && is_digit_c(s[i + 1]))) {
            return true;
        }
    }
    return false;
}

/* APPROXIMATE port of "^" + compound_callsign_pattern, full-string match.
 * Real grammar: (?:@?|\b)(X)(Y{0,2})(/)?(Y{0,3})(/)?(Y{0,3})\b
 * where X = [A-Z0-9/@], Y = [A-Z0-9/]. This checks charset + slash count
 * + max length as a shape gate; isValidCompoundCallsign does the real
 * semantic validation afterward. Revisit once that function is available. */
bool match_compound_callsign_full(const char *s) {
    size_t n = strlen(s);
    if (n == 0 || n > 11) return false;

    int slashCount = 0;
    for (size_t i = 0; i < n; i++) {
        char c = s[i];
        bool okChar = is_upper_alpha(c) || is_digit_c(c) || c == '/' ||
                      (i == 0 && c == '@');
        if (!okChar) return false;
        if (c == '/') slashCount++;
    }
    return slashCount <= 2;
}

/* Port of grid_pattern check used in parseCallsigns. Note the original
 * uses m.match(callsign).hasMatch() with NO full-length check — so it's
 * "does a grid-shaped substring appear anywhere", not a full match. */
bool contains_grid_pattern(const char *s) {
    size_t n = strlen(s);
    for (size_t i = 0; i + 4 <= n; i++) {
        if (s[i] >= 'A' && s[i] <= 'X' &&
            s[i + 1] >= 'A' && s[i + 1] <= 'X' &&
            is_digit_c(s[i + 2]) && is_digit_c(s[i + 3])) {
            return true;
        }
    }
    return false;
}

bool miniJS8_isBasecall(const char *callsign) {
    for (int i = 0; basecalls[i] != NULL; i++) {
        if (strcmp(basecalls[i], callsign) == 0) return true;
    }
    return false;
}

bool miniJS8_basecallLookup(const char *callsign, uint32_t *outCode) {
    for (size_t i = 0; basecalls[i]!=NULL; i++) {
        if (strcmp(basecalls[i], callsign) == 0) {
            *outCode = i+1+MINIJS8_NBASECALL; //I hope this works
            return true;
        }
    }
    return false;
}

bool miniJS8_startsWithCQ(const char *text) {
    for (int i = 0; cqs[i] != NULL; i++) {
        if (starts_with(text, cqs[i])) return true;
    }
    return false;
}

bool miniJS8_startsWithHB(const char *text) {
    return starts_with(text, "HB");
}

bool miniJS8_isGridLocator(const char *token) {
    size_t len = strlen(token);
    if (len < 4) return false;
    // just check the first 4-char block; extended repeated blocks per the
    // regex are rare in practice for JS8's use case
    return is_grid_block(token, len, false);
}


/* ============================================================
 * STUBS — to be ported from the corresponding C++ functions.
 * Signatures are my best-guess translation; adjust once you
 * share the real implementations (esp. isValidCallsign, which
 * parseCallsigns depends on).
 * ============================================================ */

/* Varicode::isCompoundCallsign */
/* Port of isValidCompoundCallsign(QStringView).*/
bool miniJS8_isValidCompoundCallsign(const char *callsign) {
    size_t len = strlen(callsign);
    size_t slashCount = count_char(callsign, '/');

    /* compound calls cannot be > 9 characters after removing the '/' */
    if (len - slashCount > 9) {
        return false;
    }

    /* case 2: actual compound call (contains '/') that is not a base call */
    const char *slashPos = strchr(callsign, '/');
    if (slashPos != NULL) {
        size_t prefixLen = (size_t)(slashPos - callsign);
        char prefix[16];
        if (prefixLen >= sizeof(prefix)) {
            prefixLen = sizeof(prefix) - 1; /* defensive truncation */
        }
        memcpy(prefix, callsign, prefixLen);
        prefix[prefixLen] = '\0';

        return !miniJS8_isBasecall(prefix);
    }

    /* case 1: group call */
    if (callsign[0] == '@') {
        return true;
    }

    /* case 3: arbitrary token, but with a digit/letter adjacency and
     * length > 2, so short/plain words don't get coded as callsigns */
    if (len > 2 && has_digit_letter_transition(callsign)) {
        return true;
    }

    return false;
}

bool miniJS8_isCompoundCallsign(const char *callsign) {
    if (miniJS8_isBasecall(callsign) && callsign[0] != '@') {
        return false;
    }
    if (match_base_callsign_full(callsign)) {
        return false;
    }
    if (!match_compound_callsign_full(callsign)) {
        return false;
    }
    return miniJS8_isValidCompoundCallsign(callsign);
}

bool miniJS8_isValidCallsign(const char *callsign, bool *pIsCompound) {
    if (miniJS8_isBasecall(callsign)) {
        if (pIsCompound) *pIsCompound = false;
        return true;
    }

    if (match_base_callsign_full(callsign)) {
        if (pIsCompound) *pIsCompound = false;
        return strlen(callsign) > 2 && has_digit_letter_transition(callsign);
    }

    if (match_compound_callsign_full(callsign)) {
        bool isValid = miniJS8_isValidCompoundCallsign(callsign);
        if (pIsCompound) *pIsCompound = isValid;
        return isValid;
    }

    if (pIsCompound) *pIsCompound = false;
    return false;
}

/* returns true and fills *outCode if cmd is a known directed command */
bool directed_cmds_lookup(const char *cmd, int *outCode) {
    for (size_t i = 0; i < DIRECTED_CMDS_COUNT; i++) {
        if (strcmp(directed_cmds[i].cmd, cmd) == 0) {
            *outCode = directed_cmds[i].code;
            return true;
        }
    }
    return false;
}

/* Varicode::buffered_cmds — QSet<int> membership test */
bool miniJS8_isBufferedCode(int code) {
    for (size_t i = 0; i < sizeof(buffered_cmds) / sizeof(buffered_cmds[0]); i++) {
        if (buffered_cmds[i] == code) return true;
    }
    return false;
}

/* Varicode::checksum_cmds — QMap<int,int> lookup, code -> checksum size */
bool checksum_cmds_lookup(int code, int *outSize) {
    for (size_t i = 0; i < sizeof(checksum_cmds) / sizeof(checksum_cmds[0]); i++) {
        if (checksum_cmds[i].code == code) {
            *outSize = checksum_cmds[i].size;
            return true;
        }
    }
    return false;
}

bool miniJS8_isCommandBuffered(const char *cmd) {
    int code;
    if (!directed_cmds_lookup(cmd, &code)) {
        return false;
    }
    /* NOTE: ported exactly as written, including the redundancy — since
     * nearly every key in directed_cmds contains a literal space (they're
     * all prefixed with " "), `cmd.contains(" ")` is true for almost every
     * match here, making the buffered_cmds check largely moot in practice.
     * This mirrors the original's actual (if perhaps unintended) behavior
     * rather than "fixing" it. */
    return (strchr(cmd, ' ') != NULL) || miniJS8_isBufferedCode(code);
}

int miniJS8_isCommandChecksumed(const char *cmd) {
    int code;
    if (!directed_cmds_lookup(cmd, &code)) {
        return 0;
    }
    int size;
    if (!checksum_cmds_lookup(code, &size)) {
        return 0;
    }
    return size;
}


/* TODO -- STUB: needed from you (Varicode::allowed_cmds, a QSet<int>).
 * Returning true unconditionally for now so isCommandAllowed doesn't
 * silently block every directed message during dev/testing -- replace
 * before relying on this for anything real. */
bool miniJS8_isAllowedCode(int code) {
    for (int i = 0; i<(sizeof(allowed_cmds)/sizeof(int)); i++){
        if (allowed_cmds[i] == code){
            return true;
        }
    }
    return false;
}

bool miniJS8_isAllowedCommand(const char *cmd) {
    int code;
    if (!directed_cmds_lookup(cmd, &code)) return false;
    return miniJS8_isAllowedCode(code);
}

bool miniJS8_huffLookup(char ch, uint16_t *outCode, uint8_t *outLen) {
    for (size_t i = 0; i < HUFFTABLE_COUNT; i++) {
        if (hufftable[i].ch == ch) {
            *outCode = hufftable[i].code;
            *outLen = hufftable[i].len;
            return true;
        }
    }
    return false;
}

bool miniJS8_huffIsValidUpper(char ch) {
    uint16_t code; uint8_t len;
    return miniJS8_huffLookup((char)((ch >= 'a' && ch <= 'z') ? ch - 32 : ch), &code, &len);
}

#define LONG_MAX 2147483647
#define LONG_MIN -2147483648

bool parser_isspace(char c){
    return c==' ';
}

long parser_strtol(const char *nptr, char **endptr, int base) {
    const char *p = nptr, *endp;
    bool is_neg = 0, overflow = 0;
    /* Need unsigned so (-LONG_MIN) can fit in these: */
    unsigned long n = 0UL, cutoff;
    int cutlim;
    if (base < 0 || base == 1 || base > 36) {
        return 0L;
    }
    endp = nptr;
    while (parser_isspace(*p))
        p++;
    if (*p == '+') {
        p++;
    } else if (*p == '-') {
        is_neg = 1, p++;
    }
    if (*p == '0') {
        p++;
        /* For strtol(" 0xZ", &endptr, 16), endptr should point to 'x';
         * pointing to ' ' or '0' is non-compliant.
         * (Many implementations do this wrong.) */
        endp = p;
        if (base == 16 && (*p == 'X' || *p == 'x')) {
            p++;
        } else if (base == 2 && (*p == 'B' || *p == 'b')) {
            /* C23 standard supports "0B" and "0b" prefixes. */
            p++;
        } else if (base == 8 && (*p == 'O' || *p == 'o')) {
            /* C2y standard will support "0O" and "0o" prefixes. */
            p++;
        } else if (base == 0) {
            if (*p == 'X' || *p == 'x') {
                base = 16, p++;
            } else if (*p == 'B' || *p == 'b') {
                base = 2, p++;
            } else {
                base = 8;
                if (*p == 'O' || *p == 'o') {
                    p++;
                }
            }
        }
    } else if (base == 0) {
        base = 10;
    }
    cutoff = (is_neg) ? -(LONG_MIN / base) : LONG_MAX / base;
    cutlim = (is_neg) ? -(LONG_MIN % base) : LONG_MAX % base;
    while (1) {
        int c;
        if (*p >= 'A')
            c = ((*p - 'A') & (~('a' ^ 'A'))) + 10;
        else if (*p <= '9')
            c = *p - '0';
        else
            break;
        if (c < 0 || c >= base)
            break;
        endp = ++p;
        if (overflow) {
            /* endptr should go forward and point to the non-digit character
             * (of the given base); required by ANSI standard. */
            if (endptr)
                continue;
            break;
        }
        if (n > cutoff || (n == cutoff && c > cutlim)) {
            overflow = 1;
            continue;
        }
        n = n * base + c;
    }
    if (endptr)
        *endptr = (char *)endp;
    if (overflow) {
        return ((is_neg) ? LONG_MIN : LONG_MAX);
    }
    return (long)((is_neg) ? -n : n);
}