#ifndef JS8_PARSER_H
#define JS8_PARSER_H

#include <stdint.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <malloc.h>
#include <string.h>
#include "miniJS8_const.h"

//parser-specific helpers - is there a way to make these only visible to within miniJS8?
size_t count_char(const char *s, char c);
void buf_lstrip(char *buf);
void buf_rstrip_if_nonempty(char *buf);
void buf_consume(char *buf, size_t n);
bool buf_prepend(char *buf, size_t bufsize, const char *prefix);
bool buf_append(char *buf, size_t bufsize, const char *suffix);
bool starts_with(const char *s, const char *prefix);
bool is_upper_alpha(char c);
bool is_digit_c(char c);
bool is_alnum_call(char c);
size_t miniJS8_parseCallsigns(const char *input, char out[][16], size_t max_out);
bool match_base_callsign_full(const char *s);
bool has_digit_letter_transition(const char *s);
bool match_compound_callsign_full(const char *s);
bool contains_grid_pattern(const char *s);
bool miniJS8_isBasecall(const char *callsign);
bool miniJS8_basecallLookup(const char *callsign, uint32_t *outCode);
bool miniJS8_startsWithCQ(const char *text);
bool miniJS8_startsWithHB(const char *text);

/* grid_pattern check from parseCallsigns — hand-rolled, see earlier msg */
bool miniJS8_isGridLocator(const char *token);
bool miniJS8_isValidCompoundCallsign(const char *callsign);
bool miniJS8_isCompoundCallsign(const char *callsign);
bool miniJS8_isValidCallsign(const char *callsign, bool *pIsCompound);
bool directed_cmds_lookup(const char *cmd, int *outCode);
bool miniJS8_isBufferedCode(int code);
bool checksum_cmds_lookup(int code, int *outSize);

int miniJS8_isCommandChecksumed(const char *cmd);
bool miniJS8_isCommandBuffered(const char *cmd);

bool miniJS8_isAllowedCode(int code);
bool miniJS8_isAllowedCommand(const char *cmd);
bool miniJS8_huffLookup(char ch, uint16_t *outCode, uint8_t *outLen);
bool miniJS8_huffIsValidUpper(char ch);
long parser_strtol(const char *nptr, char **endptr, int base);

#endif