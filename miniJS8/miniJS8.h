#ifndef MINIJS8_H
#define MINIJS8_H

#include "miniJS8_const.h"

miniJS8_FrameList miniJS8_buildFrames(
    const char *mycall,
    const char *mygrid,
    const char *selectedCall,
    const char *text,
    bool forceIdentify,
    bool forceData,
    int submode,
    miniJS8_MessageInfo *pInfo);

void miniJS8_freeFrames(miniJS8_FrameList *list);

void miniJS8_encode(miniJS8_Frame *frame, const miniJS8_costas_t *costas, uint8_t *tones);

void miniJS8_init();

//double miniJS8_get_symbol_hz(double base_freq, uint8_t symbol, enum miniJS8_submodes submode);
#endif