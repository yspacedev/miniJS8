# MiniJS8 - A JS8 encoding library for embedded systems

Much of the code in this library was ported from the official JS8Call-improved repo (primarily `Varicode.cpp`, `JS8.cpp`, and `Modulator.cpp`) by Claude Sonnet 5.

It currently supports all JS8Call encoding features except for compression.

## Usage

### Including

Add `#include "miniJS8/miniJS8.h"` to your code after copying the `miniJS8` folder to your code's root directory.

### Interface

MiniJS8 exposes 4 functions to interact with the library:

```c
miniJS8_FrameList miniJS8_buildFrames(
const char *mycall,
const char *mygrid,
const char *selectedCall,
const char *text,
bool forceIdentify,
bool forceData,
int submode,
miniJS8_MessageInfo *pInfo);
```

`miniJS8_buildFrames` constructs a `miniJS8_FrameList` struct consisting of an array of `miniJS8_Frame`s and the number of frames the given data was encoded into. Each `miniJS8_Frame` contains a 12-character message and a 3-bit message type used when encoding into tones to determine where a given message is in a compound message.

- `mycall` - Transmitter callsign string
- `mygrid` - Transmitter 4-character grid square
- `selectedCall` - Callsign to send a directed message to
- `text` - string of data to send
- `forceIdentify` - force every generated frame to include a callsign
- `forceData` - force usage of data mode when transmitting
- `pInfo` - information about directed calls

```c
void miniJS8_encode(miniJS8_Frame *frame, const miniJS8_costas_t *costas, uint8_t *tones);
```

`miniJS8_encode` takes a messge, its corresponding costas array, and a pointer to a `JS8_NUM_SYMBOLS` array to encode it into a sequence of transmission symbols.

- `frame` - `miniJS8_Frame` struct to encode into symbols
- `costas` - one of two JS8 costas arrays used for synchronization
- `tones` - a pointer to a `JS8_NUM_SYMBOLS` length array of `uint8_t`

```c
void miniJS8_freeFrames(miniJS8_FrameList *list);
```

`miniJS8_freeFrames` frees the memory associated with the encoded frames.

- `list` - pointer to the `miniJS8_FrameList` with pointers to the `miniJS8_Frame`s to be freed.

```c
void miniJS8_init();
```

`miniJS8_init()` initializes several arrays and structures required for operation since C does not have flexible compile-time operations

### Example

```c
//in reality, there would be much less waiting since the microcontroller
//can sleep when the transmission is occurring
double base_freq = 14079500.0; //14.079500 MHz (JS8 20m band)
miniJS8_MessageInfo info;
enum miniJS8_submodes submode = SUBMODE_NORMAL; //use 15-second transmit cycle JS8
miniJS8_FrameList list = miniJS8_buildFrames(
                        "KI5ZHW", "EL29", "", "J1Y ACK",
                        false, false, submode, &info);
for (int i = 0; i<list.count; i++){
    uint8_t out_symbols[JS8_NUM_SYMBOLS];
    const miniJS8_costas_t *tx_costas = &costas_arrays[miniJS8_submodes[submode].costas];
    miniJS8_encode(&list.frames[i], tx_costas, out_symbols);
    example_sleep(miniJS8_submodes[submode].startDelayMS); //sleep for a number of ms before transmission
    for (int j = 0; j<JS8_NUM_SYMBOLS; j++){
        //a better embedded approach is to use a lookup table of PLL divider values that
        //correspond to each symbol, but this is a better demonstration
        double tone_freq = base_freq + miniJS8_submodes[submode].toneSpacing * out_symbols[j];
        example_freq_synth(tone_freq);
        example_sleep(miniJS8_submodes[submode].toneDurationMS);
    }
    //transmitting time is miniJS8_submodes[submode].txDuration;
    example_freq_stop();
    example_wait_for_next_window(); //wait for next period to begin transmission
}
miniJS8_freeFrames(&list);
```

## Notes

Compression is not currently supported.

test.c is a simple test suite for the library and various functions within it that can be compiled with the included makefile.

The library still needs to be improved by moving some functions around, but it is still fully functional.