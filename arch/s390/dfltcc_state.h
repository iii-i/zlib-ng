#ifndef DFLTCC_STATE_H
#define DFLTCC_STATE_H

#include "zbuild.h"

/*
   Parameter Block for Query Available Functions.
 */
struct dfltcc_qaf_param {
    char fns[16];
    char reserved1[8];
    char fmts[2];
    char reserved2[6];
};

#define DFLTCC_FMT0 0

/*
   Parameter Block for Generate Dynamic-Huffman Table, Compress and Expand.
 */
#define CVT_CRC32 0
#define CVT_ADLER32 1
#define HTT_FIXED 0
#define HTT_DYNAMIC 1

struct dfltcc_param_v0 {
    uint16_t pbvn;                     /* Parameter-Block-Version Number */
    uint8_t mvn;                       /* Model-Version Number */
    uint8_t ribm;                      /* Reserved for IBM use */
    uint32_t reserved32 : 31;
    uint32_t cf : 1;                   /* Continuation Flag */
    uint8_t reserved64[8];
    uint32_t nt : 1;                   /* New Task */
    uint32_t reserved129 : 1;
    uint32_t cvt : 1;                  /* Check Value Type */
    uint32_t reserved131 : 1;
    uint32_t htt : 1;                  /* Huffman-Table Type */
    uint32_t bcf : 1;                  /* Block-Continuation Flag */
    uint32_t bcc : 1;                  /* Block Closing Control */
    uint32_t bhf : 1;                  /* Block Header Final */
    uint32_t reserved136 : 1;
    uint32_t reserved137 : 1;
    uint32_t dhtgc : 1;                /* DHT Generation Control */
    uint32_t reserved139 : 5;
    uint32_t reserved144 : 5;
    uint32_t sbb : 3;                  /* Sub-Byte Boundary */
    uint8_t oesc;                      /* Operation-Ending-Supplemental Code */
    uint32_t reserved160 : 12;
    uint32_t ifs : 4;                  /* Incomplete-Function Status */
    uint16_t ifl;                      /* Incomplete-Function Length */
    uint8_t reserved192[8];
    uint8_t reserved256[8];
    uint8_t reserved320[4];
    uint16_t hl;                       /* History Length */
    uint32_t reserved368 : 1;
    uint16_t ho : 15;                  /* History Offset */
    uint32_t cv;                       /* Check Value */
    uint32_t eobs : 15;                /* End-of-block Symbol */
    uint32_t reserved431: 1;
    uint8_t eobl : 4;                  /* End-of-block Length */
    uint32_t reserved436 : 12;
    uint32_t reserved448 : 4;
    uint16_t cdhtl : 12;               /* Compressed-Dynamic-Huffman Table
                                          Length */
    uint8_t reserved464[6];
    uint8_t cdht[288];                 /* Compressed-Dynamic-Huffman Table */
    uint8_t reserved[24];
    uint8_t ribm2[8];                  /* Reserved for IBM use */
    uint8_t csb[1152];                 /* Continuation-State Buffer */
};

/*
   Extension of inflate_state and deflate_state.
 */
struct dfltcc_state {
    struct dfltcc_param_v0 param;      /* Parameter block. */
    struct dfltcc_qaf_param af;        /* Available functions. */
    char msg[64];                      /* Buffer for strm->msg */
} ALIGNED_(8);

#endif
