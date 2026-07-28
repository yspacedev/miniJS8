//constants and structs for miniJS8

#include "miniJS8_const.h"

const char miniJS8_alphabet72[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz-+/?.";
const char miniJS8_alphanumeric[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ /@";
const char miniJS8_alphabet[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ+-./?";

const miniJS8_data_t miniJS8_submodes[5] = {
    {"NORMAL", JS8A_SYMBOL_SAMPLES, JS8A_START_DELAY_MS, JS8A_TX_SECONDS, COSTAS_ORIGINAL},
    {"FAST",   JS8B_SYMBOL_SAMPLES, JS8B_START_DELAY_MS, JS8B_TX_SECONDS, COSTAS_MODIFIED},
    {"TURBO",  JS8C_SYMBOL_SAMPLES, JS8C_START_DELAY_MS, JS8C_TX_SECONDS, COSTAS_MODIFIED},
    {"SLOW",   JS8E_SYMBOL_SAMPLES, JS8E_START_DELAY_MS, JS8E_TX_SECONDS, COSTAS_MODIFIED},
    {"ULTRA",  JS8I_SYMBOL_SAMPLES, JS8I_START_DELAY_MS, JS8I_TX_SECONDS, COSTAS_MODIFIED},
};

const miniJS8_costas_t costas_arrays[2] = {
    {{{4, 2, 5, 6, 1, 3, 0},
             {4, 2, 5, 6, 1, 3, 0},
             {4, 2, 5, 6, 1, 3, 0}}},

    {{{0, 6, 2, 3, 5, 4, 1},
             {1, 5, 0, 2, 3, 6, 4},
             {2, 5, 0, 6, 4, 1, 3}}}
};

const int allowed_cmds[33] = {-1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31};

const char *basecalls[] = {
    "<....>", "@ALLCALL", "@JS8NET",
    "@DX/NA", "@DX/SA", "@DX/EU", "@DX/AS", "@DX/AF", "@DX/OC", "@DX/AN",
    "@REGION/1", "@REGION/2", "@REGION/3",
    "@GROUP/0", "@GROUP/1", "@GROUP/2", "@GROUP/3", "@GROUP/4",
    "@GROUP/5", "@GROUP/6", "@GROUP/7", "@GROUP/8", "@GROUP/9",
    "@COMMAND", "@CONTROL", "@NET", "@NTS",
    "@RESERVE/0", "@RESERVE/1", "@RESERVE/2", "@RESERVE/3", "@RESERVE/4",
    "@APRSIS", "@RAGCHEW", "@JS8", "@EMCOMM", "@ARES", "@MARS", "@AMRRON",
    "@RACES", "@RAYNET", "@RADAR", "@SKYWARN", "@CQ", "@HB", "@QSO",
    "@QSOPARTY", "@CONTEST", "@FIELDDAY", "@SOTA", "@IOTA", "@POTA",
    "@QRP", "@QRO",
    NULL
};

const char *cqs[] = {
    "CQ CQ CQ", "CQ DX", "CQ QRP", "CQ CONTEST",
    "CQ FIELD", "CQ FD", "CQ CQ", "CQ",
    NULL
};

const int buffered_cmds[8] = {5, 9, 10, 11, 12, 13, 15, 24};

const checksum_cmd_t checksum_cmds[] = {
        {  5, 16 },
        {  9, 16 },
        { 10, 16 },
        { 11, 16 },
        { 12, 16 },
        { 13, 16 },
        { 15,  0 },
        { 24, 16 },
    };

const DirectedCmdEntry directed_cmds[] = {
    {" HEARTBEAT",     -1},
    {" HB",            -1},
    {" CQ",            -1},

    {" SNR?",           0},
    {"?",               0},

    {" DIT DIT",        1},

    {" HEARING?",       3},

    {" GRID?",          4},

    {">",               5},

    {" STATUS?",        6},

    {" STATUS",         7},

    {" HEARING",        8},

    {" MSG",            9},

    {" MSG TO:",       10},

    {" QUERY",         11},

    {" QUERY MSGS",    12},
    {" QUERY MSGS?",   12},

    {" QUERY CALL",    13},

    {" GRID",          15},

    {" INFO?",         16},
    {" INFO",          17},

    {" FB",            18},
    {" HW CPY?",       19},
    {" SK",            20},
    {" RR",            21},

    {" QSL?",          22},
    {" QSL",           23},

    {" CMD",           24},

    {" SNR",           25},
    {" NO",            26},
    {" YES",           27},
    {" 73",            28},

    {" NACK",           2},
    {" ACK",           14},

    {" HEARTBEAT SNR", 29},

    {" AGN?",          30},
    {"  ",             31},
    {" ",              31},
};

const HuffEntry hufftable[44] = {
    {' ',1,2}, {'E',4,3}, {'T',13,4}, {'A',3,4}, {'O',31,5}, {'I',28,5},
    {'N',23,5}, {'S',20,5}, {'H',3,5}, {'R',0,5}, {'D',59,6}, {'L',51,6},
    {'C',49,6}, {'U',45,6}, {'M',43,6}, {'W',11,6}, {'F',9,6}, {'G',5,6},
    {'Y',3,6}, {'P',123,7}, {'B',121,7}, {'.',116,7}, {'V',101,7}, {'K',100,7},
    {'-',97,7}, {'+',96,7}, {'?',89,7}, {'!',88,7}, {'"',85,7}, {'X',84,7},
    {'0',21,7}, {'J',20,7}, {'1',17,7}, {'Q',16,7}, {'2',9,7}, {'Z',8,7},
    {'3',5,7}, {'5',4,7}, {'4',245,8}, {'9',244,8}, {'8',241,8}, {'6',240,8},
    {'7',235,8}, {'/',234,8},
};

const char miniJS8_parityHexRows[87][23] = {
    "23bba830e23b6b6f50982e", "1f8e55da218c5df3309052", "ca7b3217cd92bd59a5ae20",
    "56f78313537d0f4382964e", "6be396b5e2e819e373340c", "293548a138858328af4210",
    "cb6c6afcdc28bb3f7c6e86", "3f2a86f5c5bd225c961150", "849dd2d63673481860f62c",
    "56cdaec6e7ae14b43feeee", "04ef5cfa3766ba778f45a4", "c525ae4bd4f627320a3974",
    "41fd9520b2e4abeb2f989c", "7fb36c24085a34d8c1dbc4", "40fc3e44bb7d2bb2756e44",
    "d38ab0a1d2e52a8ec3bc76", "3d0f929ef3949bd84d4734", "45d3814f504064f80549ae",
    "f14dbf263825d0bd04b05e", "db714f8f64e8ac7af1a76e", "8d0274de71e7c1a8055eb0",
    "51f81573dd4049b082de14", "d8f937f31822e57c562370", "b6537f417e61d1a7085336",
    "ecbd7c73b9cd34c3720c8a", "3d188ea477f6fa41317a4e", "1ac4672b549cd6dba79bcc",
    "a377253773ea678367c3f6", "0dbd816fba1543f721dc72", "ca4186dd44c3121565cf5c",
    "29c29dba9c545e267762fe", "1616d78018d0b4745ca0f2", "fe37802941d66dde02b99c",
    "a9fa8e50bcb032c85e3304", "83f640f1a48a8ebc0443ea", "3776af54ccfbae916afde6",
    "a8fc906976c35669e79ce0", "f08a91fb2e1f78290619a8", "cc9da55fe046d0cb3a770c",
    "d36d662a69ae24b74dcbd8", "40907b01280f03c0323946", "d037db825175d851f3af00",
    "1bf1490607c54032660ede", "0af7723161ec223080be86", "eca9afa0f6b01d92305edc",
    "7a8dec79a51e8ac5388022", "9059dfa2bb20ef7ef73ad4", "6abb212d9739dfc02580f2",
    "f6ad4824b87c80ebfce466", "d747bfc5fd65ef70fbd9bc", "612f63acc025b6ab476f7c",
    "05209a0abb530b9e7e34b0", "45b7ab6242b77474d9f11a", "6c280d2a0523d9c4bc5946",
    "f1627701a2d692fd9449e6", "8d9071b7e7a6a2eed6965e", "bf4f56e073271f6ab4bf80",
    "c0fc3ec4fb7d2bb2756644", "57da6d13cb96a7689b2790", "a9fa2eefa6f8796a355772",
    "164cc861bdd803c547f2ac", "cc6de59755420925f90ed2", "a0c0033a52ab6299802fd2",
    "b274db8abd3c6f396ea356", "97d4169cb33e7435718d90", "81cfc6f18c35b1e1f17114",
    "481a2a0df8a23583f82d6c", "081c29a10d468ccdbcecb6", "2c4142bf42b01e71076acc",
    "a6573f3dc8b16c9d19f746", "c87af9a5d5206abca532a8", "012dee2198eba82b19a1da",
    "b1ca4ea2e3d173bad4379c", "b33ec97be83ce413f9acc8", "5b0f7742bca86b8012609a",
    "37d8e0af9258b9e8c5f9b2", "35ad3fb0faeb5f1b0c30dc", "6114e08483043fd3f38a8a",
    "cd921fdf59e882683763f6", "95e45ecd0135aca9d6e6ae", "2e547dd7a05f6597aac516",
    "14cd0f642fc0c5fe3a65ca", "3a0a1dfd7eee29c2e827e0", "c8b5dffc335095dcdcaf2a",
    "3dd01a59d86310743ec752", "8abdb889efbe39a510a118", "3f231f212055371cf3e2a2"};