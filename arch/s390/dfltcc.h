#ifndef DFLTCC_H
#define DFLTCC_H

#ifdef ZLIB_COMPAT
#include "zlib.h"
#else
#include "zlib-ng.h"
#endif
#include "zutil.h"

void ZLIB_INTERNAL *dfltcc_alloc_state(PREFIX3(streamp) strm, uInt items, uInt size);
void ZLIB_INTERNAL dfltcc_copy_state(void *dst, const void *src, uInt size);
void ZLIB_INTERNAL dfltcc_reset(PREFIX3(streamp) strm, uInt size);
void ZLIB_INTERNAL *dfltcc_alloc_window(PREFIX3(streamp) strm, uInt items, uInt size);
void ZLIB_INTERNAL dfltcc_free_window(PREFIX3(streamp) strm, void *w);
int ZLIB_INTERNAL dfltcc_can_inflate(PREFIX3(streamp) strm);
typedef enum {
    DFLTCC_INFLATE_CONTINUE,
    DFLTCC_INFLATE_BREAK,
    DFLTCC_INFLATE_SOFTWARE,
} dfltcc_inflate_action;
dfltcc_inflate_action ZLIB_INTERNAL dfltcc_inflate(PREFIX3(streamp) strm, int flush, int *ret);
int ZLIB_INTERNAL dfltcc_was_inflate_used(PREFIX3(streamp) strm);
int ZLIB_INTERNAL dfltcc_inflate_disable(PREFIX3(streamp) strm);

#define ZALLOC_STATE dfltcc_alloc_state

#define ZFREE_STATE ZFREE

#define ZCOPY_STATE dfltcc_copy_state

#define ZALLOC_WINDOW dfltcc_alloc_window

#define ZFREE_WINDOW dfltcc_free_window

#define TRY_FREE_WINDOW dfltcc_free_window

#define INFLATE_RESET_KEEP_HOOK(strm) \
    dfltcc_reset((strm), sizeof(struct inflate_state))

#define INFLATE_PRIME_HOOK(strm, bits, value) \
    do { if (dfltcc_inflate_disable((strm))) return Z_STREAM_ERROR; } while (0)

#define INFLATE_TYPEDO_HOOK(strm, flush) \
    if (dfltcc_can_inflate((strm))) { \
        dfltcc_inflate_action action; \
\
        RESTORE(); \
        action = dfltcc_inflate((strm), (flush), &ret); \
        LOAD(); \
        if (action == DFLTCC_INFLATE_CONTINUE) \
            break; \
        else if (action == DFLTCC_INFLATE_BREAK) \
            goto inf_leave; \
    }

#define INFLATE_NEED_CHECKSUM(strm) (!dfltcc_can_inflate((strm)))

#define INFLATE_NEED_UPDATEWINDOW(strm) (!dfltcc_can_inflate((strm)))

#define INFLATE_MARK_HOOK(strm) \
    do { \
        if (dfltcc_was_inflate_used((strm))) return -(1L << 16); \
    } while (0)

#endif
